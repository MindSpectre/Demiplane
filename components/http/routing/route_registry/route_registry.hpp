#pragma once

#include <array>
#include <cstddef>
#include <demiplane/gears>
#include <functional>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <errors.hpp>
#include <http_enums.hpp>
#include <middleware.hpp>

namespace demiplane::http {

    /// Path normalization policy (spec §8.6), applied at registration AND at
    /// lookup so both sides agree on the canonical form. Owned by the registry
    /// in PR 2; ServerConfig (PR 6) maps its config enum onto it.
    enum class PathNormalization : std::uint8_t {
        none,                     ///< exact byte match
        collapse_trailing_slash,  ///< "/users/" == "/users"   (default)
        collapse_multi_slash,     ///< + "/users//42" == "/users/42"
    };

    /// Build-phase registration conflict. Aggregated by freeze(); the Server
    /// (PR 5) turns a non-empty set into a RouteConflictAggregateError throw.
    struct RouteConflictError {
        HttpMethod method;
        std::string path;
        std::string detail;
    };

    /// Thrown by Server::setup() when freeze() reports conflicts: every
    /// duplicate (method, path) registration in ONE exception, so
    /// misconfigurations surface all at once, not piecemeal (spec §8.7).
    class RouteConflictAggregateError final : public std::runtime_error {
    public:
        explicit RouteConflictAggregateError(std::vector<RouteConflictError> conflicts)
            : std::runtime_error{format_message(conflicts)},
              conflicts_{std::move(conflicts)} {
        }

        [[nodiscard]] const std::vector<RouteConflictError>& conflicts() const noexcept {
            return conflicts_;
        }

    private:
        [[nodiscard]] static std::string format_message(const std::vector<RouteConflictError>& conflicts);

        std::vector<RouteConflictError> conflicts_;
    };

    /// One matched route: a pointer to the frozen registry's stored handler —
    /// NEVER a copy (copying a std::function may heap-allocate, spec §8.5) —
    /// plus the captured path parameters. Param NAME views point into the
    /// registry's route templates (stable while the registry lives); VALUE
    /// views point into the incoming path or the request arena (valid until
    /// the arena resets).
    struct ResolvedRoute {
        using ParamVec = boost::container::small_vector<
            std::pair<std::string_view, std::string_view>,
            4,
            std::pmr::polymorphic_allocator<std::pair<std::string_view, std::string_view>>>;

        const ContextHandler* handler;
        ParamVec path_params;
    };

    /// Join a group prefix and a route path with slash hygiene:
    /// ("", "/users") -> "/users"; ("/api/", "/users") -> "/api/users";
    /// ("/api", "/") -> "/api". Build-phase helper (heap OK).
    [[nodiscard]] std::string join_path(std::string_view prefix, std::string_view path);

    /**
     * @brief Method+path route table: exact + parametric, baked at startup,
     *        immutable — and therefore safely shared across io threads —
     *        after freeze() (spec §8.1/§8.5).
     *
     * Parametric syntax: a whole segment of the form "{name}" captures the
     * incoming segment ("/users/{id}/posts/{post_id}"). Captures percent-
     * decode with plus_is_space=false into the request arena; literal
     * segments compare raw bytes, so '%' (and '{'/'}') are rejected inside
     * literal segments at registration (spec §8.6 split-before-decode).
     *
     * Lifecycle: add_route() during build → freeze() → find_route() only.
     */
    class RouteRegistry : gears::NonCopyable {
    public:
        explicit RouteRegistry(const PathNormalization norm = PathNormalization::collapse_trailing_slash) noexcept
            : norm_{norm} {
        }

        /// Throws std::logic_error after freeze(); std::invalid_argument on
        /// HttpMethod::unknown, a null handler, a path not starting with '/',
        /// '%'/'{'/'}' inside a literal segment, or an empty/duplicate
        /// parameter name. A duplicate (method, normalized path) does NOT
        /// throw — it is recorded and reported by freeze() so the caller sees
        /// every conflict at once (spec §8.7).
        void add_route(HttpMethod method, std::string_view path, ContextHandler handler);

        /// Freezes the registry; returns all recorded conflicts (empty = OK).
        [[nodiscard]] std::vector<RouteConflictError> freeze();

        [[nodiscard]] bool is_frozen() const noexcept {
            return frozen_;
        }

        /// Hot path: zero global-heap allocations on a match (gated). Exact
        /// match wins over parametric. 404 vs 405 per spec §8.5: a known path
        /// with no handler for `method` yields MethodNotAllowedError carrying
        /// the populated verb set.
        [[nodiscard]] gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>
        find_route(HttpMethod method, std::string_view path, std::pmr::polymorphic_allocator<> arena_alloc) const;

    private:
        struct PathSegment {
            std::string text;  ///< literal text, or the parameter name
            bool is_param = false;
        };
        using MethodSlots = std::array<ContextHandler, HTTP_METHOD_COUNT>;

        struct ParamTemplate {
            std::vector<PathSegment> segments;
            MethodSlots by_method;
        };

        /// Heterogeneous lookup: find(string_view) without a temp std::string.
        struct TransparentStringHash {
            using is_transparent = void;
            [[nodiscard]] std::size_t operator()(const std::string_view s) const noexcept {
                return std::hash<std::string_view>{}(s);
            }
        };

        [[nodiscard]] std::string normalize_owned(std::string_view path) const;
        [[nodiscard]] std::string_view normalize_lookup(std::string_view path,
                                                        std::pmr::polymorphic_allocator<> alloc) const;
        [[nodiscard]] static std::vector<PathSegment> parse_segments(std::string_view normalized);
        /// Matches `path` against `tmpl`, appending captures to out_params in
        /// segment order. out_params is NOT cleared on a false return — the
        /// caller must pass a fresh (or cleared) vector per template attempt.
        [[nodiscard]] static bool match_template(const ParamTemplate& tmpl,
                                                 std::string_view path,
                                                 std::pmr::polymorphic_allocator<> alloc,
                                                 ResolvedRoute::ParamVec& out_params);
        [[nodiscard]] static std::vector<HttpMethod> allowed_methods(const MethodSlots& slots);

        bool frozen_ = false;
        PathNormalization norm_;
        boost::unordered::unordered_flat_map<std::string, MethodSlots, TransparentStringHash, std::equal_to<>> exact_;
        std::vector<ParamTemplate> parametric_;
        std::vector<RouteConflictError> conflicts_;
    };

}  // namespace demiplane::http
