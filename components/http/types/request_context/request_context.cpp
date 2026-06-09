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
        bool has(std::string_view hay, std::string_view needle) {
            return hay.find(needle) != std::string_view::npos;
        }
    }

    bool RequestContext::is_json()      const { auto ct = header("content-type"); return ct && has(*ct, "application/json"); }
    bool RequestContext::is_form()      const { auto ct = header("content-type"); return ct && has(*ct, "application/x-www-form-urlencoded"); }
    bool RequestContext::is_multipart() const { auto ct = header("content-type"); return ct && has(*ct, "multipart/form-data"); }
    bool RequestContext::accepts_json() const { auto a = header("accept"); return a && (has(*a, "application/json") || has(*a, "*/*")); }
    bool RequestContext::accepts_html() const { auto a = header("accept"); return a && (has(*a, "text/html") || has(*a, "*/*")); }

}  // namespace demiplane::http
