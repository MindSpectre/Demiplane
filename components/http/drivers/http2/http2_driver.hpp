#pragma once

#include <demiplane/scroll>
#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace demiplane::http {

    /// HTTP/2 driver — SCAFFOLD (spec §6.4). serve() logs and closes; fill in
    /// with nghttp2 in a future PR (vcpkg dep already in the manifest). Satisfies
    /// HttpDriver so the TLS listener can carry it via ALPN.
    class Http2Driver {
    public:
        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http2;
        }

        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"h2"};
            return ALPNS;
        }

        template <IsStreamConnection ConnT>
        AsyncVoid serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http2Driver::serve() not implemented (scaffold)";
            gears::force_non_const(this);
            co_await conn.async_close();
        }

    private:
        SCROLL_COMPONENT_PREFIX("Http2Driver");
    };

}  // namespace demiplane::http
