#pragma once

#include <demiplane/gears>
#include <string_view>

#include <body.hpp>
#include <headers.hpp>
#include <http_enums.hpp>

namespace demiplane::http {

    /**
     * @brief Protocol-agnostic HTTP request.
     *
     * `target` is the raw, undecoded request target as a VIEW into the receive
     * buffer (e.g. "/users/42?q=foo"). It — like the headers and body — is valid
     * only while the owning connection's buffers live, i.e. for the duration of
     * the handler. Copy out anything you keep. Path/query split + URL decode
     * happen in RequestContext.
     *
     * `headers` and `body` are move-only value types. A Request must be
     * constructed with a Headers bound to an allocator (there is no null state).
     */
    struct Request : gears::NonCopyable {
        HttpMethod method   = HttpMethod::unknown;
        HttpVersion version = HttpVersion::http_1_1;
        std::string_view target;  // view into receive buffer
        Headers headers;          // move-only; bound to an allocator
        Body body;                // value type; default EmptyBody

        explicit Request(Headers hdrs)
            : headers{std::move(hdrs)} {
        }
    };

}  // namespace demiplane::http
