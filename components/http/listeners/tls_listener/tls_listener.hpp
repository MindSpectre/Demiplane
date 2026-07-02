#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/error.hpp>
#include <build_ssl_context.hpp>
#include <connection_tracker.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tls_config.hpp>
#include <tls_connection.hpp>

namespace demiplane::http {

    /**
     * @brief TLS listener for one or more drivers, multiplexed by ALPN (spec §7.3).
     *
     * Advertises the union of the drivers' accepted_alpns(); build_ssl_context
     * installs the ALPN select callback (D4). On each accept it constructs a
     * TlsConnection on a per-connection strand (D7), handshakes, and dispatches
     * to the driver whose id() matches the negotiated protocol (compile-time
     * tuple walk — spike S2). Server-preference order = template arg order.
     *
     * LIFETIME CONTRACT: drain_until(...) before destroying the listener (the
     * spawned coroutines hold a tracker Handle into this listener + serve via
     * `this`). Arena size is fixed at the 8 KB default in v1 (PR 6 wires
     * request_arena_size).
     * // TODO(PR5): drain_until dispatches force-cancels but does not await their unwind; "safe to destroy after drain"
     * holds only on a single-threaded executor in v1. See ConnectionTracker::drain_until.
     */
    template <IsHttpDriver... Drivers>
    class TlsListener final : public ListenerBase {
        static_assert(sizeof...(Drivers) >= 1, "TlsListener needs at least one driver");

    public:
        TlsListener(boost::asio::any_io_executor exec,
                    std::string host,
                    const std::uint16_t port,
                    TlsConfig tls,
                    Drivers... drivers)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              tls_config_{std::move(tls)},
              drivers_{std::move(drivers)...},
              advertised_alpn_{build_alpn_wire()},
              acceptor_{exec_} {
        }

        void bind() override {
            // Build the context FIRST — a cert/key error throws before we bind a
            // socket (no half-open listener left behind).
            ctx_.emplace(build_ssl_context(tls_config_, advertised_alpn_));

            const boost::asio::ip::tcp::endpoint ep{boost::asio::ip::make_address(host_), port_};
            acceptor_.open(ep.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(ep);
            acceptor_.listen(boost::asio::socket_base::max_listen_connections);
        }

        boost::asio::awaitable<void> run(Router& router) override {
            namespace asio = boost::asio;
            for (;;) {
                auto strand = asio::make_strand(exec_);
                boost::beast::error_code ec;
                asio::ip::tcp::socket sock =
                    co_await acceptor_.async_accept(strand, asio::redirect_error(asio::use_awaitable, ec));
                if (ec == asio::error::operation_aborted) {
                    break;
                }
                if (ec) {
                    continue;
                }
                auto conn   = std::make_shared<TlsConnection>(std::move(sock), *ctx_);
                auto handle = tracker_.register_connection(conn, strand);
                asio::co_spawn(
                    strand,
                    [this, &router, conn, h = std::move(handle)]() -> asio::awaitable<void> {
                        if (co_await conn->handshake()) {  // ALPN mismatch / TLS failure → close
                            co_await conn->async_close();
                            co_return;
                        }
                        if (const bool handled = co_await try_serve<0>(*conn, router, conn->negotiated_protocol());
                            !handled) {  // unreachable after a successful ALPN
                            co_await conn->async_close();
                        }
                    },
                    asio::detached);
            }
            // Refuse new connections during shutdown (spec §14.2; spike S4) — see
            // TcpListener::run for the rationale.
            boost::beast::error_code ignore;
            std::ignore = acceptor_.close(ignore);
            co_return;
        }

        boost::asio::awaitable<void> drain_until(const std::chrono::steady_clock::time_point deadline) override {
            co_await tracker_.drain_until(exec_, deadline);
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return acceptor_.local_endpoint().port();
        }

    private:
        // Concatenate every driver's ALPN ids into the wire format (len-prefixed).
        static std::string build_alpn_wire() {
            std::string wire;
            (append_alpns<Drivers>(wire), ...);
            return wire;
        }

        template <IsHttpDriver D>
        static void append_alpns(std::string& wire) {
            for (const std::string_view alpn : D::accepted_alpns()) {
                wire.push_back(static_cast<char>(alpn.size()));
                wire.append(alpn);
            }
        }

        // Walk the driver tuple; the first whose id() == proto serves the conn.
        template <std::size_t I>
        boost::asio::awaitable<bool> try_serve(TlsConnection& conn, Router& router, Protocol proto) {
            if constexpr (I < sizeof...(Drivers)) {
                if (auto& drv = std::get<I>(drivers_); std::remove_reference_t<decltype(drv)>::id() == proto) {
                    co_await drv.serve(conn, router);
                    co_return true;
                }
                co_return co_await try_serve<I + 1>(conn, router, proto);
            } else {
                co_return false;
            }
        }

        boost::asio::any_io_executor exec_;
        std::string host_;
        std::uint16_t port_;
        TlsConfig tls_config_;
        std::tuple<Drivers...> drivers_;
        std::string advertised_alpn_;  // outlives ctx_ (ALPN cb arg, D4)
        std::optional<boost::asio::ssl::context> ctx_;
        boost::asio::ip::tcp::acceptor acceptor_;
        ConnectionTracker tracker_;
    };

}  // namespace demiplane::http
