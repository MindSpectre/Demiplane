#include "route_registry.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

#include <url_decode.hpp>

namespace demiplane::http {

    namespace {
        /// Calls fn(segment) for each '/'-separated segment of `path` (which
        /// must start with '/'). "/" yields one empty segment — harmless and,
        /// critically, CONSISTENT between registration and lookup. Stops and
        /// returns false the first time fn returns false.
        template <typename Fn>
        bool for_each_segment(const std::string_view path, Fn&& fn) {
            std::size_t pos = 1;  // skip the leading '/'
            while (pos <= path.size()) {
                const std::size_t next = path.find('/', pos);
                const std::string_view seg =
                    path.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos);
                if (!fn(seg))
                    return false;
                if (next == std::string_view::npos)
                    break;
                pos = next + 1;
            }
            return true;
        }
    }  // namespace

    std::string join_path(const std::string_view prefix, const std::string_view path) {
        std::string out{prefix};
        while (!out.empty() && out.back() == '/')
            out.pop_back();
        if (path.empty() || path == "/")
            return out.empty() ? std::string{"/"} : out;
        if (path.front() != '/')
            out.push_back('/');
        out += path;
        return out;
    }

    std::string RouteRegistry::normalize_owned(const std::string_view path) const {
        std::string out;
        out.reserve(path.size());
        if (norm_ == PathNormalization::collapse_multi_slash) {
            bool prev_slash = false;
            for (const char c : path) {
                if (c == '/' && prev_slash)
                    continue;
                prev_slash = c == '/';
                out.push_back(c);
            }
        } else {
            out.assign(path);
        }
        if (norm_ != PathNormalization::none) {
            if (out.size() > 1 && out.back() == '/')
                out.pop_back();
        }
        return out;
    }

    std::vector<RouteRegistry::PathSegment> RouteRegistry::parse_segments(const std::string_view normalized) {
        std::vector<PathSegment> out;
        std::vector<std::string_view> seen_names;
        for_each_segment(normalized, [&](const std::string_view seg) {
            if (seg.size() >= 2 && seg.front() == '{' && seg.back() == '}') {
                const std::string_view name = seg.substr(1, seg.size() - 2);
                if (name.empty())
                    throw std::invalid_argument{"RouteRegistry: empty parameter name in route '" +
                                                std::string{normalized} + "'"};
                if (name.find_first_of("{}") != std::string_view::npos)
                    throw std::invalid_argument{"RouteRegistry: nested braces in route '" + std::string{normalized} +
                                                "'"};
                if (std::ranges::find(seen_names, name) != seen_names.end())
                    throw std::invalid_argument{"RouteRegistry: duplicate parameter name '" + std::string{name} +
                                                "' in route '" + std::string{normalized} + "'"};
                seen_names.push_back(name);
                out.push_back(PathSegment{std::string{name}, true});
            } else {
                if (seg.find_first_of("%{}") != std::string_view::npos)
                    throw std::invalid_argument{"RouteRegistry: '%', '{' and '}' are not allowed in literal route "
                                                "segments ('" +
                                                std::string{normalized} + "', spec §8.6)"};
                out.push_back(PathSegment{std::string{seg}, false});
            }
            return true;
        });
        return out;
    }

    void RouteRegistry::add_route(const HttpMethod method, const std::string_view path, ContextHandler handler) {
        if (frozen_)
            throw std::logic_error{"RouteRegistry: registration after freeze()"};
        if (method == HttpMethod::unknown)
            throw std::invalid_argument{"RouteRegistry: cannot register HttpMethod::unknown"};
        if (!handler)
            throw std::invalid_argument{"RouteRegistry: null handler"};
        if (path.empty() || path.front() != '/')
            throw std::invalid_argument{"RouteRegistry: route path must start with '/': '" + std::string{path} + "'"};

        std::string normalized            = normalize_owned(path);
        std::vector<PathSegment> segments = parse_segments(normalized);
        const bool parametric = std::ranges::any_of(segments, [](const PathSegment& s) { return s.is_param; });
        const auto idx        = std::to_underlying(method);

        if (!parametric) {
            auto [it, inserted] = exact_.try_emplace(std::move(normalized));
            if (it->second[idx]) {
                conflicts_.push_back(RouteConflictError{method, it->first, "duplicate route"});
                return;
            }
            it->second[idx] = std::move(handler);
            return;
        }

        const auto same_shape = [](const std::vector<PathSegment>& a, const std::vector<PathSegment>& b) {
            return std::ranges::equal(a, b, [](const PathSegment& x, const PathSegment& y) {
                return x.is_param == y.is_param && (x.is_param || x.text == y.text);
            });
        };
        for (auto& [param_segments, param_by_method] : parametric_) {
            if (!same_shape(param_segments, segments))
                continue;
            const bool names_match = std::ranges::equal(
                param_segments, segments, [](const PathSegment& x, const PathSegment& y) { return x.text == y.text; });
            if (!names_match) {
                conflicts_.push_back(
                    RouteConflictError{method,
                                       std::move(normalized),
                                       "parametric route already registered with different parameter names"});
                return;
            }
            if (param_by_method[idx]) {
                conflicts_.push_back(RouteConflictError{method, std::move(normalized), "duplicate route"});
                return;
            }
            param_by_method[idx] = std::move(handler);
            return;
        }
        ParamTemplate tmpl;
        tmpl.segments       = std::move(segments);
        tmpl.by_method[idx] = std::move(handler);
        parametric_.push_back(std::move(tmpl));
    }

    std::vector<RouteConflictError> RouteRegistry::freeze() {
        frozen_ = true;
        return std::exchange(conflicts_, {});
    }

    // ── Lookup side ─────────────────────────────────────────────────────────

    std::string_view RouteRegistry::normalize_lookup(std::string_view path [[clang::lifetimebound]],
                                                     std::pmr::polymorphic_allocator<> alloc) const {
        if (norm_ == PathNormalization::none)
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return path;
        if (norm_ == PathNormalization::collapse_multi_slash && path.find("//") != std::string_view::npos) {
            // Rare path: rewrite into the request arena — never the global heap.
            const auto buf  = static_cast<char*>(alloc.allocate_bytes(path.size(), 1));
            std::size_t n   = 0;
            bool prev_slash = false;
            for (const char c : path) {
                if (c == '/' && prev_slash)
                    continue;
                prev_slash = c == '/';
                buf[n++]   = c;
            }
            path = std::string_view{buf, n};
            // collapse_trailing_slash (and collapse_multi_slash after run-collapse):
            // strip exactly one trailing slash — a view shrink, zero alloc.
            if (path.size() > 1 && path.back() == '/')
                path.remove_suffix(1);
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return path;
        }
        // collapse_trailing_slash (and collapse_multi_slash after run-collapse):
        // strip exactly one trailing slash — a view shrink, zero alloc.
        if (path.size() > 1 && path.back() == '/')
            path.remove_suffix(1);
        // ReSharper disable once CppDFALocalValueEscapesFunction
        return path;
    }

    bool RouteRegistry::match_template(const ParamTemplate& tmpl,
                                       const std::string_view path,
                                       const std::pmr::polymorphic_allocator<> alloc,
                                       ResolvedRoute::ParamVec& out_params) {
        std::size_t i     = 0;
        const bool walked = for_each_segment(path, [&](const std::string_view seg) {
            if (i >= tmpl.segments.size())
                return false;  // more incoming segments than the template has
            const auto& [text, is_param] = tmpl.segments[i++];
            if (!is_param)
                return seg == text;  // literal: raw byte compare (§8.6)
            if (seg.empty())
                return false;  // a param never captures an empty segment
            const auto decoded = url_decode_arena(seg, /*plus_is_space=*/false, alloc);
            if (!decoded)
                return false;  // malformed escape → this template does not match
            out_params.emplace_back(std::string_view{text}, *decoded);
            return true;
        });
        return walked && i == tmpl.segments.size();
    }

    std::vector<HttpMethod> RouteRegistry::allowed_methods(const MethodSlots& slots) {
        std::vector<HttpMethod> out;                         // cold path (405) — heap is fine here
        for (std::size_t i = 1; i < HTTP_METHOD_COUNT; ++i)  // slot 0 = unknown, never filled
            if (slots[i])
                out.push_back(static_cast<HttpMethod>(i));
        return out;
    }

    gears::Outcome<ResolvedRoute, NotFoundError, MethodNotAllowedError>
    RouteRegistry::find_route(const HttpMethod method,
                              const std::string_view path,
                              const std::pmr::polymorphic_allocator<> arena_alloc) const {
        assert(frozen_ && "RouteRegistry::find_route on an unfrozen registry");
        const std::string_view normalized = normalize_lookup(path, arena_alloc);
        const auto idx                    = std::to_underlying(method);

        if (const auto it = exact_.find(normalized); it != exact_.end()) {
            if (const ContextHandler& h = it->second[idx]) {
                return ResolvedRoute{&h, ResolvedRoute::ParamVec{arena_alloc}};
            }
            return gears::err(MethodNotAllowedError{allowed_methods(it->second)});
        }

        for (const ParamTemplate& tmpl : parametric_) {
            ResolvedRoute::ParamVec params{arena_alloc};
            if (!match_template(tmpl, normalized, arena_alloc, params))
                continue;
            if (const ContextHandler& h = tmpl.by_method[idx]) {
                return ResolvedRoute{&h, std::move(params)};
            }
            // First shape match decides 405 (spec §8.5) — no fall-through.
            return gears::err(MethodNotAllowedError{allowed_methods(tmpl.by_method)});
        }
        return gears::err(NotFoundError{"route", std::string{normalized}});
    }

}  // namespace demiplane::http
