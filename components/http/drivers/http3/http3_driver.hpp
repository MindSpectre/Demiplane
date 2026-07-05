#pragma once

#include <demiplane/scroll>
#include <span>
#include <string_view>

#include <boost/asio/awaitable.hpp>
#include <connection_concepts.hpp>
#include <http_enums.hpp>
#include <router.hpp>

namespace demiplane::http {

    /// HTTP/3 driver — SCAFFOLD (spec §6.4). serve() logs and closes; fill in
    /// with ngtcp2 + nghttp3 in a future PR (vcpkg deps already in the manifest).
    /// Pairs with QuicConnection (QUIC transport) when implemented.
    class Http3Driver {
    public:
        [[nodiscard]] static constexpr Protocol id() noexcept {
            return Protocol::http3;
        }

        [[nodiscard]] static constexpr std::span<const std::string_view> accepted_alpns() noexcept {
            static constexpr std::string_view ALPNS[] = {"h3"};
            return ALPNS;
        }

        template <IsConnection ConnT>
        boost::asio::awaitable<void> serve(ConnT& conn, Router& /*router*/) {
            COMPONENT_LOG_WRN() << "Http3Driver::serve() not implemented (scaffold)";
            gears::force_non_const(this);
            co_await conn.async_close();
        }

    private:
        SCROLL_COMPONENT_PREFIX("Http3Driver");
    };

}  // namespace demiplane::http
