#pragma once

#include <cstddef>
#include <demiplane/gears>
#include <iterator>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <boost/beast/http/fields.hpp>

namespace demiplane::http {

    /**
     * @brief The Beast fields type an incoming request is parsed into.
     *
     * pmr, not `std::allocator`: Beast allocates one node per header line plus
     * one for the request target, and with the default allocator every one of
     * those is a global-heap malloc/free per request (~4 measured). Binding it
     * to the per-connection request arena makes them bump allocations that the
     * arena reset reclaims for free.
     *
     * This is a single concrete type on purpose. Templating `Headers` on the
     * fields allocator would push the parameter through BeastBacking, the
     * iterator, and every consumer, for no benefit — nothing constructs Headers
     * over a differently-allocated fields object.
     */
    using BeastFields = boost::beast::http::basic_fields<std::pmr::polymorphic_allocator<char>>;

    /**
     * @brief Multi-value, case-insensitive, insertion-ordered HTTP headers.
     *
     * Two backings behind one API:
     *   BeastBacking — read-only view over a parser-owned BeastFields
     *                  (incoming requests, zero copy).
     *   OwnedBacking — arena-owned pmr::string pairs (responses, h2/h3 incoming,
     *                  synthetic/test). ALWAYS carries its allocator.
     *
     * There is no default/null state: construct via owned(alloc) or
     * view_of_beast(fields). Mutators and promotion allocate through the bound
     * allocator — never the global heap.
     */
    class Headers : gears::NonCopyable {
    public:
        using value_type = std::pair<std::string_view, std::string_view>;

        // ── Factories ────────────────────────────────────────────────────
        /// Allocates the entry vector's first block through `alloc`; the only
        /// possible throw is an unrecoverable bad_alloc
        /// (GEARS_UNRECOVERABLE_NOEXCEPT — terminate by default).
        static Headers owned(std::pmr::polymorphic_allocator<> alloc) GEARS_UNRECOVERABLE_NOEXCEPT;
        /// Non-owning view. `fields` MUST outlive the returned Headers.
        static Headers view_of_beast(const BeastFields& fields);

        /// Beast's basic_fields has an implicit converting ctor from a
        /// differently-allocated basic_fields. Without these deletions,
        /// `view_of_beast(some_http_fields)` would materialize a temporary
        /// BeastFields, take its address, and dangle the moment the full
        /// expression ends — a segfault, not a compile error. Ask for it and get
        /// a diagnostic instead.
        template <class OtherAlloc>
        static Headers view_of_beast(const boost::beast::http::basic_fields<OtherAlloc>&) = delete;
        static Headers view_of_beast(BeastFields&&)                                       = delete;

        // Move-only (it may hold a pmr container). Move-ASSIGN is user-defined:
        // it adopts the source's backing wholesale (variant emplace → vector
        // move-construct steals buffer + allocator). A defaulted move-assign
        // hits the pmr POCMA=false trap — element-wise COPY into the
        // destination's OLD allocator, so `Response r; r = co_await next(ctx);`
        // would silently copy arena headers onto the global heap.
        Headers(Headers&&) = default;
        Headers& operator=(Headers&& other) noexcept;

        // ── Read API ─────────────────────────────────────────────────────
        [[nodiscard]] std::optional<std::string_view> get(std::string_view name) const;
        [[nodiscard]] std::string get_or(std::string_view name, std::string_view fallback) const;
        [[nodiscard]] std::pmr::vector<std::string_view> get_all(std::string_view name,
                                                                 std::pmr::polymorphic_allocator<> alloc) const;
        [[nodiscard]] bool contains(std::string_view name) const;
        [[nodiscard]] std::size_t size() const;
        [[nodiscard]] bool empty() const {
            return size() == 0;
        }

        // ── Write API (requires OwnedBacking; promote first if viewing) ────
        void add(std::string_view name, std::string_view value);
        void set(std::string_view name, std::string_view value);
        void remove(std::string_view name);

        /// Copy a BeastBacking into a fresh OwnedBacking in `alloc`. No-op if
        /// already owned. MUST be called before mutating a viewing Headers.
        void promote_to_owned(std::pmr::polymorphic_allocator<> alloc);

        // ── Iteration (O(1) per step) ─────────────────────────────────────
        class const_iterator {
        public:
            using value_type        = value_type;
            using reference         = value_type;
            using difference_type   = std::ptrdiff_t;
            // input, not forward: operator* returns a proxy pair by value.
            using iterator_category = std::input_iterator_tag;

            const_iterator() = default;
            value_type operator*() const;
            const_iterator& operator++();
            const_iterator operator++(int);
            friend bool operator==(const const_iterator& a, const const_iterator& b) {
                return a.idx_ == b.idx_ && a.h_ == b.h_;
            }

        private:
            friend class Headers;
            const Headers* h_ = nullptr;
            std::size_t idx_  = 0;  // position; also drives beast_it_ advance
            BeastFields::const_iterator beast_it_{};
        };

        [[nodiscard]] const_iterator begin() const;
        [[nodiscard]] const_iterator end() const;

    private:
        struct BeastBacking {
            const BeastFields* fields;
        };
        struct OwnedBacking {
            std::pmr::vector<std::pair<std::pmr::string, std::pmr::string>> entries;
            explicit OwnedBacking(const std::pmr::polymorphic_allocator<> a)
                : entries(a) {
                // One up-front bump allocation (arena on the hot path) instead
                // of grow-reallocate-move on the first few add()s.
                entries.reserve(4);
            }
        };
        std::variant<BeastBacking, OwnedBacking> backing_;

        explicit Headers(BeastBacking b)
            : backing_{b} {
        }
        explicit Headers(OwnedBacking&& o)
            : backing_{std::move(o)} {
        }

        OwnedBacking& as_owned();  // asserts owned; used by mutators after promote
    };

}  // namespace demiplane::http
