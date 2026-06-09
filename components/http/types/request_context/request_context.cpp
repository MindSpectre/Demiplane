#include "request_context.hpp"

#include <utility>

namespace demiplane::http {

    RequestContext::RequestContext(Request req, std::pmr::polymorphic_allocator<> alloc)
        : request_{std::move(req)}, alloc_{alloc} {}

    void RequestContext::ensure_split() const {
        if (cached_path_.has_value()) return;
        std::string_view t = request_.target;
        auto q = t.find('?');
        if (q == std::string_view::npos) { cached_path_ = t; cached_query_ = std::string_view{}; }
        else { cached_path_ = t.substr(0, q); cached_query_ = t.substr(q + 1); }
    }
    std::string_view RequestContext::path() const { ensure_split(); return *cached_path_; }
    std::string_view RequestContext::query_string() const { ensure_split(); return *cached_query_; }

    namespace {
        bool contains_substr(std::string_view hay, std::string_view needle) {
            return hay.find(needle) != std::string_view::npos;
        }
    }

    bool RequestContext::is_json()      const { auto ct = header("content-type"); return ct && contains_substr(*ct, "application/json"); }
    bool RequestContext::is_form()      const { auto ct = header("content-type"); return ct && contains_substr(*ct, "application/x-www-form-urlencoded"); }
    bool RequestContext::is_multipart() const { auto ct = header("content-type"); return ct && contains_substr(*ct, "multipart/form-data"); }
    bool RequestContext::accepts_json() const { auto a = header("accept"); return a && (contains_substr(*a, "application/json") || contains_substr(*a, "*/*")); }
    bool RequestContext::accepts_html() const { auto a = header("accept"); return a && (contains_substr(*a, "text/html") || contains_substr(*a, "*/*")); }

}  // namespace demiplane::http

#include "../url_decode/url_decode.hpp"

namespace demiplane::http {

    void RequestContext::set_path_param(std::string_view name, std::string_view value) {
        path_params_.emplace_back(std::pmr::string{name, alloc_}, std::pmr::string{value, alloc_});
    }

    std::optional<std::string_view> RequestContext::raw_path_param(std::string_view name) const {
        for (const auto& [k, v] : path_params_)
            if (std::string_view(k) == name) return std::string_view(v);
        return std::nullopt;
    }

    void RequestContext::ensure_query_parsed() const {
        if (query_parsed_) return;
        query_parsed_ = true;
        std::string_view qs = query_string();
        std::size_t i = 0;
        while (i < qs.size()) {
            std::size_t amp = qs.find('&', i);
            std::string_view pair{qs.data() + i, (amp == std::string_view::npos ? qs.size() - i : amp - i)};
            std::size_t eq = pair.find('=');
            std::string_view rk = (eq == std::string_view::npos) ? pair : pair.substr(0, eq);
            std::string_view rv = (eq == std::string_view::npos) ? std::string_view{} : pair.substr(eq + 1);
            auto k = url_decode(rk), v = url_decode(rv);
            if (k && v) query_params_.emplace_back(std::pmr::string{*k, alloc_}, std::pmr::string{*v, alloc_});
            // Malformed escapes are skipped here; a handler wanting strict
            // parsing uses body().read_form().
            if (amp == std::string_view::npos) break;
            i = amp + 1;
        }
    }

    std::optional<std::string_view> RequestContext::raw_query(std::string_view name) const {
        ensure_query_parsed();
        for (const auto& [k, v] : query_params_)
            if (std::string_view(k) == name) return std::string_view(v);
        return std::nullopt;
    }

}  // namespace demiplane::http

namespace demiplane::http {

    RequestContext::~RequestContext() {
        for (auto& e : bag_) if (e.destroyer && e.ptr) e.destroyer(e.ptr);
    }

    namespace {
        Response build(std::pmr::polymorphic_allocator<> a, HttpStatus s,
                       std::string body, std::string_view ct, bool with_ct) {
            Response r{a};                       // alloc + headers bound to the arena
            r.status = s;
            if (with_ct) r.add_header("Content-Type", ct);
            if (!body.empty()) r.body = Body::owned(std::move(body));
            return r;
        }
    }

    Response RequestContext::ok(std::string b, std::string_view ct)      { return build(alloc_, HttpStatus::ok, std::move(b), ct, true); }
    Response RequestContext::json(std::string b)                         { return build(alloc_, HttpStatus::ok, std::move(b), "application/json", true); }
    Response RequestContext::created(std::string b, std::string_view ct) { return build(alloc_, HttpStatus::created, std::move(b), ct, true); }
    Response RequestContext::no_content()                                { return build(alloc_, HttpStatus::no_content, "", "", false); }
    Response RequestContext::status(HttpStatus s, std::string b, std::string_view ct) { return build(alloc_, s, std::move(b), ct, true); }

    Response RequestContext::redirect(std::string_view location, HttpStatus s) {
        Response r{alloc_};
        r.status = s;
        r.add_header("Location", location);
        return r;
    }

}  // namespace demiplane::http
