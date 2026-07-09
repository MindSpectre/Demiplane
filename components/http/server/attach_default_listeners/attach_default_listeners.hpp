#pragma once

namespace demiplane::http {

    class Server;

    /**
     * @brief Walk server.config().listeners() and add the matching listener +
     *        driver instances (spec §10.3) — the config-driven wiring path.
     *
     * Driver config derives from the server-level config: Http11Config gets
     * body_limit + the three phase timeouts (max_header_bytes keeps its 16 KB
     * struct default — no ServerConfig field maps to it in v1).
     *
     * v1-supported (transport, protocols) combinations:
     *   tcp  + [http1]                     → TcpListener<Http11Driver>
     *   tls  + [http1]                     → TlsListener<Http11Driver>
     *   tls  + [http2]                     → TlsListener<Http2Driver>   (scaffold driver)
     *   tls  + [http1, http2] (either order) → TlsListener<…> in the LISTED
     *                                        order — JSON order is the ALPN
     *                                        server-preference order
     *   quic + [http3]                     → QuicListener<Http3Driver>  (scaffold)
     * An empty protocols list defaults per transport (http1; http3 on quic).
     *
     * Call during the build phase (before setup()). An empty listeners array
     * is a no-op — programmatic add_*_listener calls compose with this.
     *
     * @throws std::invalid_argument on a combination v1 cannot serve
     *         (e.g. tcp+[http2] — h2c is not supported).
     */
    void attach_default_listeners(Server& server);

}  // namespace demiplane::http
