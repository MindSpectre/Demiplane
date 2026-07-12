#pragma once

#include <charconv>
#include <memory_resource>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>

#include <body.hpp>
#include <boost/container/small_vector.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
#include <request.hpp>
#include <response.hpp>

namespace demiplane::http {

    /**
     * @brief Handler-facing view of one in-flight request.
     *
     * Owns the moved-in Request + the request arena allocator. Header lookup is
     * lazy. The target→(path, query) split is memoized; since `target` is a VIEW
     * into stable connection-owned storage, the cached views survive moves (the
     * old owned-string SSO dangle is gone). Move-only; passed by value into
     * handlers. Valid only for the handler's duration.
     */
    class RequestContext {
    public:
        RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc);

        RequestContext(RequestContext&&)                 = default;
        // Move-ASSIGN is deleted: a defaulted one would replace bag_ without
        // running the old payloads' destructors (a real leak for owning
        // payloads). Contexts are move-CONSTRUCTED through the chain; nothing
        // ever assigns over one.
        RequestContext& operator=(RequestContext&&)      = delete;
        RequestContext(const RequestContext&)            = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        HttpMethod method() const noexcept {
            return request_.method;
        }
        HttpVersion version() const noexcept {
            return request_.version;
        }
        std::string_view target() const noexcept {
            return request_.target;
        }
        std::string_view path() const;
        std::string_view query_string() const;
        const Headers& headers() const noexcept {
            return request_.headers;
        }
        Body& body() noexcept {
            return request_.body;
        }

        std::optional<std::string_view> header(const std::string_view name) const {
            return request_.headers.get(name);
        }
        std::string header_or(const std::string_view name, const std::string_view fallback) const {
            return request_.headers.get_or(name, fallback);
        }

        bool is_json() const;
        bool is_form() const;
        bool is_multipart() const;
        bool accepts_json() const;
        bool accepts_html() const;

        std::pmr::polymorphic_allocator<> arena_alloc() const noexcept {
            return alloc_;
        }

        // ── Path parameters (set by the routing layer, PR2) ───────────────
        void set_path_param(std::string_view name, std::string_view value);

        template <typename T>
        std::optional<T> path_param(const std::string_view name) const {
            auto raw = raw_path_param(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T path_param_or(const std::string_view name, T fallback) const {
            if (auto v = path_param<T>(name))
                return *std::move(v);
            return fallback;
        }

        // ── Query parameters (lazily parsed from query_string) ────────────
        template <typename T>
        std::optional<T> query(const std::string_view name) const {
            auto raw = raw_query(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T query_or(const std::string_view name, T fallback) const {
            if (auto v = query<T>(name))
                return *std::move(v);
            return fallback;
        }

        // ── Type-keyed middleware bag (arena-backed) ──────────────────────
        template <typename T>
        void set(T value) {
            static_assert(std::is_move_constructible_v<T>);
            const std::type_index key{typeid(T)};
            if (auto* e = find_bag_entry(key)) {
                // Construct the new payload BEFORE destroying the old one, so a
                // throwing move leaves the existing payload intact (strong
                // guarantee) rather than a live destroyer over destroyed bytes.
                void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
                ::new (mem) T(std::move(value));
                e->destroyer(e->ptr);
                e->ptr       = mem;
                e->destroyer = +[](void* p) noexcept { static_cast<T*>(p)->~T(); };
                return;
            }
            void* mem = alloc_.allocate_bytes(sizeof(T), alignof(T));
            ::new (mem) T(std::move(value));
            bag_.push_back(BagEntry{key, mem, +[](void* p) noexcept { static_cast<T*>(p)->~T(); }});
        }
        template <typename T>
        T* get() {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<T*>(e->ptr) : nullptr;
        }
        template <typename T>
        const T* get() const {
            auto* e = find_bag_entry(std::type_index{typeid(T)});
            return e ? static_cast<const T*>(e->ptr) : nullptr;
        }
        template <typename T>
        bool has() const {
            return find_bag_entry(std::type_index{typeid(T)}) != nullptr;
        }

        // ── Arena-bound response factories (hot path; spec §5.4) ──────────
        Response ok(std::string body = "", std::string_view ct = "text/plain");
        Response json(std::string body);
        Response created(std::string body = "", std::string_view ct = "application/json");
        Response no_content();
        Response redirect(std::string_view location, HttpStatus status = HttpStatus::found);
        Response status(HttpStatus s, std::string body = "", std::string_view ct = "text/plain");

        ~RequestContext();

    private:
        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        using ParamEntry = std::pair<std::pmr::string, std::pmr::string>;
        // Inline capacity 1, not 4 (README Finding 15): ParamEntry is 64 B, so
        // 4-slot inline storage on TWO vectors plus the bag put ~600 B inside
        // every RequestContext — paid on each by-value move through the
        // handler chain and in every handler coroutine frame. Exact-match
        // routes carry ZERO entries; overflow beyond 1 lands in the request
        // arena (bump alloc), so multi-param routes pay one cheap arena grow.
        using ParamVec   = boost::container::small_vector<ParamEntry, 1, std::pmr::polymorphic_allocator<ParamEntry>>;

        ParamVec path_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};
        mutable bool query_parsed_ = false;
        mutable ParamVec query_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

        struct BagEntry {
            std::type_index key;
            void* ptr;
            void (*destroyer)(void*) noexcept;

            BagEntry(const std::type_index k, void* p, void (*d)(void*) noexcept) noexcept
                : key{k},
                  ptr{p},
                  destroyer{d} {
            }
            // Move nulls the source ptr, so a moved-from RequestContext's bag
            // runs NO destroyers — double-destruction is impossible regardless
            // of small_vector's moved-from element behaviour. (~RequestContext
            // guards on `ptr`.) RequestContext is moved by value through the
            // middleware chain carrying a populated bag, so this path is hot.
            BagEntry(BagEntry&& o) noexcept
                : key{o.key},
                  ptr{o.ptr},
                  destroyer{o.destroyer} {
                o.ptr = nullptr;
            }
            BagEntry& operator=(BagEntry&& o) noexcept {
                key       = o.key;
                ptr       = o.ptr;
                destroyer = o.destroyer;
                o.ptr     = nullptr;
                return *this;
            }
            BagEntry(const BagEntry&)            = delete;
            BagEntry& operator=(const BagEntry&) = delete;
        };
        boost::container::small_vector<BagEntry, 1, std::pmr::polymorphic_allocator<BagEntry>> bag_{
            std::pmr::polymorphic_allocator<BagEntry>{alloc_}};

        BagEntry* find_bag_entry(const std::type_index key) {
            for (auto& e : bag_)
                if (e.key == key)
                    return &e;
            return nullptr;
        }
        const BagEntry* find_bag_entry(const std::type_index key) const {
            for (const auto& e : bag_)
                if (e.key == key)
                    return &e;
            return nullptr;
        }

        mutable std::optional<std::string_view> cached_path_;
        mutable std::optional<std::string_view> cached_query_;
        void ensure_split() const;

        void ensure_query_parsed() const;
        std::optional<std::string_view> raw_query(std::string_view name) const;
        std::optional<std::string_view> raw_path_param(std::string_view name) const;

        // Defined in the header so consumers instantiate for their own T —
        // no .cpp explicit-instantiation list, no link cap on the type set.
        template <typename T>
        static std::optional<T> convert_string(std::string_view value) {
            if constexpr (std::is_same_v<T, std::string>) {
                return std::string{value};
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                return value;
            } else if constexpr (std::is_same_v<T, bool>) {
                // is_arithmetic_v<bool> is true but from_chars has no bool
                // overload — this branch must precede the arithmetic one.
                // Strict by design: "1"/"true" and "0"/"false" only.
                if (value == "1" || value == "true")
                    return true;
                if (value == "0" || value == "false")
                    return false;
                return std::nullopt;
            } else if constexpr (std::is_arithmetic_v<T>) {
                T out{};
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
                if (ec != std::errc{} || ptr != value.data() + value.size())
                    return std::nullopt;
                return out;
            } else {
                static_assert(sizeof(T) == 0, "RequestContext: unsupported param type");
                std::unreachable();
            }
        }
    };

}  // namespace demiplane::http
