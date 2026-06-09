#pragma once

#include <charconv>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include <boost/container/small_vector.hpp>

#include "../body/body.hpp"
#include "../headers/headers.hpp"
#include "../http_enums.hpp"
#include "../request/request.hpp"

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

        RequestContext(RequestContext&&)            = default;
        // Move-ASSIGN is deleted: a defaulted one would replace bag_ without
        // running the old payloads' destructors (a real leak for owning
        // payloads). Contexts are move-CONSTRUCTED through the chain; nothing
        // ever assigns over one.
        RequestContext& operator=(RequestContext&&) = delete;
        RequestContext(const RequestContext&)            = delete;
        RequestContext& operator=(const RequestContext&) = delete;

        HttpMethod  method()  const noexcept { return request_.method; }
        HttpVersion version() const noexcept { return request_.version; }
        std::string_view target() const noexcept { return request_.target; }
        std::string_view path()         const;
        std::string_view query_string() const;
        const Headers& headers() const noexcept { return request_.headers; }
        Body& body() noexcept { return request_.body; }

        std::optional<std::string_view> header(std::string_view name) const {
            return request_.headers.get(name);
        }
        std::string header_or(std::string_view name, std::string_view fallback) const {
            return request_.headers.get_or(name, fallback);
        }

        bool is_json()      const;
        bool is_form()      const;
        bool is_multipart() const;
        bool accepts_json() const;
        bool accepts_html() const;

        std::pmr::polymorphic_allocator<> arena_alloc() const noexcept { return alloc_; }

        // ── Path parameters (set by the routing layer, PR2) ───────────────
        void set_path_param(std::string_view name, std::string_view value);

        template <typename T>
        std::optional<T> path_param(std::string_view name) const {
            auto raw = raw_path_param(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T path_param_or(std::string_view name, T fallback) const {
            if (auto v = path_param<T>(name)) return *std::move(v);
            return fallback;
        }

        // ── Query parameters (lazily parsed from query_string) ────────────
        template <typename T>
        std::optional<T> query(std::string_view name) const {
            auto raw = raw_query(name);
            return raw ? convert_string<T>(*raw) : std::nullopt;
        }
        template <typename T>
        T query_or(std::string_view name, T fallback) const {
            if (auto v = query<T>(name)) return *std::move(v);
            return fallback;
        }

    private:
        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        using ParamEntry = std::pair<std::pmr::string, std::pmr::string>;
        using ParamVec = boost::container::small_vector<
            ParamEntry, 4, std::pmr::polymorphic_allocator<ParamEntry>>;

        ParamVec path_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};
        mutable bool query_parsed_ = false;
        mutable ParamVec query_params_{std::pmr::polymorphic_allocator<ParamEntry>{alloc_}};

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
            } else if constexpr (std::is_arithmetic_v<T>) {
                T out{};
                auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), out);
                if (ec != std::errc{} || ptr != value.data() + value.size()) return std::nullopt;
                return out;
            } else {
                static_assert(sizeof(T) == 0, "RequestContext: unsupported param type");
            }
        }
    };

}  // namespace demiplane::http
