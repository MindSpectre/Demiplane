#include "attach_default_listeners.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include <http11_config.hpp>
#include <http11_driver.hpp>
#include <http2_driver.hpp>
#include <http3_driver.hpp>
#include <http_enums.hpp>
#include <listener_config.hpp>
#include <server.hpp>
#include <server_config.hpp>

namespace demiplane::http {

    namespace {

        Http11Config make_http11_config(const ServerConfig& cfg) noexcept {
            Http11Config h11{};  // max_header_bytes keeps its 16 KB default
            h11.max_body_bytes = cfg.body_limit();
            h11.header_timeout = cfg.timeouts().header();
            h11.body_timeout   = cfg.timeouts().body();
            h11.idle_timeout   = cfg.timeouts().idle();
            return h11;
        }

        [[noreturn]] void throw_unsupported(const ListenerConfig& listener, const std::vector<Protocol>& protocols) {
            std::string msg = "attach_default_listeners: unsupported (transport, protocols) combination: ";
            msg += to_string_view(listener.transport());
            msg += " + [";
            for (std::size_t i = 0; i < protocols.size(); ++i) {
                if (i != 0) {
                    msg += ", ";
                }
                msg += to_string_view(protocols[i]);
            }
            msg += "]";
            throw std::invalid_argument{msg};
        }

        void attach_one(Server& server, const ListenerConfig& listener) {
            const ServerConfig& cfg = server.config();
            const auto protocols    = listener.effective_protocols();
            const std::vector h1    = {Protocol::http1};
            const std::vector h2    = {Protocol::http2};
            const std::vector h1_h2 = {Protocol::http1, Protocol::http2};
            const std::vector h2_h1 = {Protocol::http2, Protocol::http1};
            const std::vector h3    = {Protocol::http3};

            switch (listener.transport()) {
                case ListenerConfig::Transport::tcp:
                    if (protocols == h1) {
                        server.add_tcp_listener(
                            listener.bind_address(), listener.port(), Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    break;
                case ListenerConfig::Transport::tls:
                    // ListenerConfig::validate() guarantees tls() is engaged here.
                    if (protocols == h1) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    if (protocols == h2) {
                        server.add_tls_listener(
                            listener.bind_address(), listener.port(), *listener.tls(), Http2Driver{});
                        return;
                    }
                    if (protocols == h1_h2) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http11Driver{make_http11_config(cfg)}, Http2Driver{});
                        return;
                    }
                    if (protocols == h2_h1) {
                        server.add_tls_listener(listener.bind_address(), listener.port(), *listener.tls(),
                                                Http2Driver{}, Http11Driver{make_http11_config(cfg)});
                        return;
                    }
                    break;
                case ListenerConfig::Transport::quic:
                    if (protocols == h3) {
                        server.add_quic_listener(
                            listener.bind_address(), listener.port(), *listener.tls(), Http3Driver{});
                        return;
                    }
                    break;
            }
            throw_unsupported(listener, protocols);
        }

    }  // namespace

    void attach_default_listeners(Server& server) {
        for (const ListenerConfig& listener : server.config().listeners()) {
            attach_one(server, listener);
        }
    }

}  // namespace demiplane::http
