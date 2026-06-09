#pragma once

#include <demiplane/gears>
#include <memory_resource>
#include <string>
#include <string_view>
#include <utility>

#include <body.hpp>
#include <headers.hpp>
#include <http_enums.hpp>
namespace demiplane::http {

    /**
     * @brief Protocol-agnostic HTTP response.
     *
     * STORES its allocator: built on the hot path through RequestContext
     * (Task 15) it points at the request arena, so even middleware that mutates
     * the response AFTER the handler returns keeps allocations in the arena.
     * Default-constructed (ctx-less / error / test) it uses new_delete — the
     * cold path. Body is a value type (Task 7). Drivers stamp Date/Server.
     */
    struct Response : gears::NonCopyable {
        std::pmr::polymorphic_allocator<> alloc{};
        HttpStatus status   = HttpStatus::ok;
        HttpVersion version = HttpVersion::http_1_1;
        bool keep_alive     = true;
        Headers headers;
        Body body;  // default EmptyBody

        explicit Response(const std::pmr::polymorphic_allocator<> a = {})
            : alloc{a},
              headers{Headers::owned(a)} {
        }

        Response(Response&&) = default;
        // pmr::polymorphic_allocator has a deleted copy-assign, so the
        // defaulted move-assign is also deleted. Provide one that keeps `alloc`
        // sticky (the resource is owned by the arena that outlives this struct)
        // and moves the payload fields.
        Response& operator=(Response&& other) noexcept {
            if (this != &other) {
                status     = other.status;
                version    = other.version;
                keep_alive = other.keep_alive;
                headers    = std::move(other.headers);
                body       = std::move(other.body);
                // alloc is intentionally NOT reassigned — it is sticky.
            }
            return *this;
        }

        // ── Fluent setters (deducing this; chain on lvalues + rvalues) ─────
        template <typename Self>
        auto&& with_status(this Self&& self, HttpStatus s) noexcept {
            self.status = s;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_version(this Self&& self, HttpVersion v) noexcept {
            self.version = v;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_keep_alive(this Self&& self, bool k) noexcept {
            self.keep_alive = k;
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& set_header(this Self&& self, std::string_view n, std::string_view v) {
            self.headers.set(n, v);
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& add_header(this Self&& self, std::string_view n, std::string_view v) {
            self.headers.add(n, v);
            return std::forward<Self>(self);
        }
        template <typename Self>
        auto&& with_body(this Self&& self, std::string content) {
            self.body = Body::owned(std::move(content));
            return std::forward<Self>(self);
        }
    };

}  // namespace demiplane::http
