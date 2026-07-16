/**
 * Load-test counterpart of the HTTP bomber: a minimal demiplane HTTP server
 * exposing cheap endpoints. Not a measured benchmark itself — the bomber
 * reports client-side RPS/latency; this process is the thing under load.
 *
 *   ./Demiplane.Benchmarks.Http.BenchServer [port=8080] [io_threads=4]
 *   ./Demiplane.Benchmarks.Http.Bomber 127.0.0.1 8080 /ping 4 30 30
 *
 * Ctrl+C (SIGINT/SIGTERM) triggers graceful shutdown via run_standalone.
 */
#include <cstddef>
#include <cstdint>
#include <demiplane/http>
#include <iostream>
#include <memory>
#include <string>

namespace {

    namespace http = demiplane::http;

    /// GET /ping        → 200 "pong"           (fixed body — framework floor)
    /// GET /json        → 200 {"status":"ok"}  (arena-backed JSON response)
    /// GET /users/{id}  → 200 "user <id>"      (parametric route + arena decode)
    class BenchController final : public http::HttpController {
    public:
        void configure_routes() override {
            Get("/ping", &BenchController::ping);
            Get("/json", &BenchController::json);
            Get("/users/{id}", &BenchController::user);
        }

    private:
        static http::AsyncResponse ping(http::RequestContext ctx) {
            co_return ctx.ok("pong");
        }

        static http::AsyncResponse json(http::RequestContext ctx) {
            co_return ctx.json(R"({"status":"ok"})");
        }

        static http::AsyncResponse user(http::RequestContext ctx) {
            co_return ctx.ok("user " + ctx.path_param_or<std::string>("id", std::string{"?"}));
        }
    };

}  // namespace

int main(const int argc, char* argv[]) {
    namespace http = demiplane::http;

    // Argument parsing sits inside the try: std::stoi throws on junk input,
    // and an uncaught throw out of main() is a terminate(), not a diagnostic.
    // Same shape as http_bomber.cpp's main().
    try {
        std::uint16_t port  = 8080;
        std::size_t threads = 4;
        if (argc > 1) {
            port = static_cast<std::uint16_t>(std::stoi(argv[1]));
        }
        if (argc > 2) {
            threads = static_cast<std::size_t>(std::stoi(argv[2]));
        }

        std::cout << "bench_server: http://0.0.0.0:" << port << " (" << threads
                  << " io threads) — Ctrl+C for graceful shutdown\n";

        http::run_standalone(http::ServerConfig::Builder{}.finalize(), threads, [&](http::Server& server) {
            server.add_tcp_listener("0.0.0.0", port, http::Http11Driver{http::Http11Config{}});
            server.add_controller(std::make_shared<BenchController>());
        });
    } catch (const std::exception& e) {
        std::cerr << "bench_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
