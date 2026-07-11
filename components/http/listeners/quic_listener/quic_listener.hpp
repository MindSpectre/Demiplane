#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <demiplane/scroll>
#include <string>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <executor.hpp>
#include <http3_driver.hpp>
#include <listener_base.hpp>
#include <router.hpp>
#include <tls_config.hpp>

namespace demiplane::http {

    /**
     * @brief QUIC listener — SCAFFOLD (spec §7.3, D6).
     *
     * bind() succeeds as a no-op; run() logs a warning and returns. Pairs with
     * Http3Driver ONLY (QUIC is the h3 transport). The UDP socket + ngtcp2
     * handshake land inside these methods in the h3 PR — no surrounding change.
     * Links no ngtcp2/nghttp3 symbols (continues PR 3 D4).
     */
    template <typename Driver>
    class QuicListener final : public ListenerBase {
        static_assert(std::same_as<Driver, Http3Driver>, "QuicListener pairs with Http3Driver only (spec §7.3)");

    public:
        QuicListener(Executor exec, std::string host, const std::uint16_t port, TlsConfig tls, Driver driver)
            : exec_{std::move(exec)},
              host_{std::move(host)},
              port_{port},
              tls_{std::move(tls)},
              driver_{std::move(driver)} {
        }

        void bind() override {
            // Scaffold: no socket yet. The h3 PR opens the UDP socket here.
        }

        boost::asio::awaitable<void> run([[maybe_unused]] Router& router) override {
            COMPONENT_LOG_WRN() << "QuicListener::run() not implemented (scaffold)";
            co_return;
        }

        boost::asio::awaitable<void>
        drain_until([[maybe_unused]] std::chrono::steady_clock::time_point deadline) override {
            co_return;
        }

        [[nodiscard]] std::size_t in_flight() const noexcept override {
            return 0;  // scaffold: QUIC connections are not tracked (D6)
        }

        [[nodiscard]] std::string bind_address() const override {
            return host_;
        }
        [[nodiscard]] std::uint16_t bound_port() const override {
            return port_;
        }

    private:
        Executor exec_;
        std::string host_;
        std::uint16_t port_;
        TlsConfig tls_;
        Driver driver_;
        SCROLL_COMPONENT_PREFIX("QuicListener");
    };

}  // namespace demiplane::http
