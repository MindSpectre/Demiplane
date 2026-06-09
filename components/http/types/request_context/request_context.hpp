#pragma once

#include <memory_resource>
#include <optional>
#include <string_view>

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

    private:
        Request request_;
        std::pmr::polymorphic_allocator<> alloc_;

        mutable std::optional<std::string_view> cached_path_;
        mutable std::optional<std::string_view> cached_query_;
        void ensure_split() const;
    };

}  // namespace demiplane::http
