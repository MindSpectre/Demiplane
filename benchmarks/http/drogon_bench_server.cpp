/**
 * Drogon reference server — the yardstick for bench_server.cpp.
 *
 *   ./Demiplane.Benchmarks.Http.DrogonBenchServer [port=8081] [threads=4]
 *
 * Endpoint parity with bench_server.cpp: GET /ping -> 200 "pong" (text/plain),
 * built fresh per request (bench_server.cpp does not cache a response either).
 *
 * Every fairness knob is set explicitly rather than left to a default, because
 * the defaults differ from what components/http/ can do:
 *
 *   reuse-port OFF  — TcpListener sets only reuse_address (tcp_listener.hpp:67);
 *                     it has no SO_REUSEPORT, so Drogon must not use one either.
 *   logging OFF     — the demiplane build under test compiles logging out.
 *   gzip/brotli OFF — the h1 driver does neither.
 *   Date + Server   — left ON; Http11Driver::stamp_common_headers stamps both.
 *   keep-alive      — uncapped, matching the h1 driver's session loop.
 *
 * Response bytes are equalized on purpose. Out of the box Drogon answers /ping
 * in 143 bytes and demiplane in 124: Drogon appends "; charset=utf-8" to the
 * content type and its Server token is longer. Bytes written per response is a
 * throughput input, so the harness pins both to "text/plain" and the same Server
 * token. Everything else about Drogon's response path is untouched.
 */
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <string>

#include <drogon/drogon.h>

int main(const int argc, char* argv[]) {
    try {
        std::uint16_t port  = 8081;
        std::size_t threads = 4;
        if (argc > 1)
            port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        if (argc > 2)
            threads = static_cast<std::size_t>(std::stoi(argv[2]));

        std::cout << "drogon_bench_server: http://0.0.0.0:" << port << " (" << threads
                  << " io threads) — Ctrl+C to stop\n";

        auto& app = drogon::app();

        app.registerHandler(
            "/ping",
            [](const drogon::HttpRequestPtr&, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                // Not setContentTypeCode(CT_TEXT_PLAIN) — that emits
                // "text/plain; charset=utf-8", 15 bytes more than demiplane sends.
                resp->setContentTypeString("text/plain");
                resp->setBody("pong");
                callback(resp);
            },
            {drogon::Get});

        app.setLogLevel(trantor::Logger::kFatal);
        app.setThreadNum(threads);
        app.setServerHeaderField("Demiplane");  // equalize Server-token length
        app.enableReusePort(false);             // TcpListener has no SO_REUSEPORT
        app.enableGzip(false);
        app.enableBrotli(false);
        app.setKeepaliveRequestsNumber(0);  // unlimited, like the h1 session loop
        app.setIdleConnectionTimeout(60);   // matches Http11Config::idle_timeout
        app.addListener("0.0.0.0", port, /*useSSL=*/false);

        app.run();
    } catch (const std::exception& e) {
        std::cerr << "drogon_bench_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
