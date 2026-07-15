#include "request_context.hpp"

#include <utility>

#include <url_decode.hpp>

namespace demiplane::http {

    RequestContext::RequestContext(Request req, const std::pmr::polymorphic_allocator<> alloc)
        : request_{std::move(req)},
          alloc_{alloc} {
    }

    void RequestContext::ensure_split() const {
        if (cached_path_.has_value())
            return;
        std::string_view t = request_.target;
        if (const auto q = t.find('?'); q == std::string_view::npos) {
            cached_path_  = t;
            cached_query_ = std::string_view{};
        } else {
            cached_path_  = t.substr(0, q);
            cached_query_ = t.substr(q + 1);
        }
    }
    std::string_view RequestContext::path() const {
        ensure_split();
        return *cached_path_;
    }
    std::string_view RequestContext::query_string() const {
        ensure_split();
        return *cached_query_;
    }

    namespace {
        bool contains_substr(const std::string_view hay, const std::string_view needle) {
            return hay.find(needle) != std::string_view::npos;
        }
    }  // namespace

    bool RequestContext::is_json() const {
        const auto content_type = header("content-type");
        return content_type && contains_substr(*content_type, "application/json");
    }
    bool RequestContext::is_form() const {
        const auto content_type = header("content-type");
        return content_type && contains_substr(*content_type, "application/x-www-form-urlencoded");
    }
    bool RequestContext::is_multipart() const {
        const auto content_type = header("content-type");
        return content_type && contains_substr(*content_type, "multipart/form-data");
    }
    bool RequestContext::accepts_json() const {
        const auto accept = header("accept");
        return accept && (contains_substr(*accept, "application/json") || contains_substr(*accept, "*/*"));
    }
    bool RequestContext::accepts_html() const {
        const auto accept = header("accept");
        return accept && (contains_substr(*accept, "text/html") || contains_substr(*accept, "*/*"));
    }

    // ── Path / query parameters ──────────────────────────────────────────
    void RequestContext::set_path_param(const std::string_view name, const std::string_view value) {
        path_params_.emplace_back(std::pmr::string{name, alloc_}, std::pmr::string{value, alloc_});
    }

    std::optional<std::string_view> RequestContext::raw_path_param(const std::string_view name) const {
        for (const auto& [k, v] : path_params_)
            if (std::string_view(k) == name)
                return std::string_view(v);
        return std::nullopt;
    }

    void RequestContext::ensure_query_parsed() const {
        if (query_parsed_)
            return;
        query_parsed_       = true;
        std::string_view qs = query_string();
        std::size_t i       = 0;
        while (i < qs.size()) {
            const std::size_t amp = qs.find('&', i);
            std::string_view pair{qs.data() + i, (amp == std::string_view::npos ? qs.size() - i : amp - i)};
            const std::size_t eq      = pair.find('=');
            const std::string_view rk = eq == std::string_view::npos ? pair : pair.substr(0, eq);
            const std::string_view rv = eq == std::string_view::npos ? std::string_view{} : pair.substr(eq + 1);
            auto v                    = url_decode(rv);
            if (auto k = url_decode(rk); k && v)
                query_params_.emplace_back(std::pmr::string{*k, alloc_}, std::pmr::string{*v, alloc_});
            // Malformed escapes are skipped here; a handler wanting strict
            // parsing uses body().read_form().
            if (amp == std::string_view::npos)
                break;
            i = amp + 1;
        }
    }

    std::optional<std::string_view> RequestContext::raw_query(const std::string_view name) const {
        ensure_query_parsed();
        for (const auto& [k, v] : query_params_)
            if (std::string_view(k) == name)
                return std::string_view(v);
        return std::nullopt;
    }

    // ── Type-keyed bag + ctx-scoped response factories ───────────────────
    RequestContext::~RequestContext() {
        for (const auto& e : bag_)
            if (e.destroyer && e.ptr)
                e.destroyer(e.ptr);
    }

    Response RequestContext::no_content() const {
        Response r{alloc_};
        r.status = HttpStatus::no_content;
        return r;
    }

    Response RequestContext::redirect(const std::string_view location, const HttpStatus status) const {
        Response r{alloc_};
        r.status = status;
        r.add_header("Location", location);
        return r;
    }

}  // namespace demiplane::http
