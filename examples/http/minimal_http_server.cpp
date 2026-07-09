/**
 * Minimal demiplane HTTP server (spec §10.3, standalone path).
 *
 * Two wiring paths:
 *   ./minimal_http_server                 → programmatic config, 127.0.0.1:8080
 *   ./minimal_http_server server.json     → JSON config via load_server_config
 *                                           + attach_default_listeners
 * Try:
 *   curl http://127.0.0.1:8080/api/hello/world
 *   curl http://127.0.0.1:8080/api/healthz
 *   curl -d 'payload' http://127.0.0.1:8080/api/echo
 *
 * Ctrl+C triggers graceful shutdown (accept loops cancelled, in-flight
 * requests drained, observers notified) — run_standalone owns the executor,
 * the worker threads, and the §9.7 stop → wait → teardown sequence.
 */
#include <cstddef>
#include <demiplane/http>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace {

    namespace http = demiplane::http;

    /// GET /hello/{name} — path parameter, URL-decoded into the request arena.
    /// GET /healthz      — arena-backed JSON response.
    /// POST /echo        — typed-error handler: AsyncOutcome + ADL to_http_response.
    class GreeterController final : public http::HttpController {
    public:
        void configure_routes() override {
            Get("/hello/{name}", &GreeterController::hello);
            Get("/healthz", &GreeterController::healthz);
            Post("/echo", &GreeterController::echo);
        }

    private:
        http::AsyncResponse hello(http::RequestContext ctx) {
            co_return ctx.ok("hello, " + ctx.path_param_or<std::string>("name", std::string{"world"}) + "\n");
        }

        http::AsyncResponse healthz(http::RequestContext ctx) {
            co_return ctx.json(R"({"status":"ok"})");
        }

        /// Oversize bodies short-circuit as BodyLimitExceeded; the bake layer
        /// converts the typed error into a 413 via errors.hpp's ADL
        /// to_http_response — the handler never builds an error response.
        http::AsyncOutcome<http::Response, http::BodyLimitExceeded> echo(http::RequestContext ctx) {
            auto body = co_await ctx.body().read_to_string(64 * 1024);
            if (body.is_error()) {
                co_return demiplane::gears::err(body.error<http::BodyLimitExceeded>());
            }
            co_return ctx.ok(std::move(body).value());
        }
    };

    /// Post-processing middleware: stamps a header on every response of the
    /// controller it is attached to (runs after the handler returns).
    http::Middleware server_tag_middleware() {
        return [](http::RequestContext ctx, const http::NextHandler& next) -> http::AsyncResponse {
            auto response = co_await next(std::move(ctx));
            response.headers.set("X-Example", "minimal-http-server");
            co_return response;
        };
    }

    /// Report whichever config error alternative the Outcome holds.
    [[nodiscard]] int report_config_error(const demiplane::gears::Outcome<http::ServerConfig,
                                                                          http::ConfigFileError,
                                                                          http::ConfigParseError,
                                                                          http::ConfigSchemaError>& loaded) {
        if (loaded.holds_error<http::ConfigFileError>()) {
            const auto& e = loaded.error<http::ConfigFileError>();
            std::cerr << "config: cannot read " << e.path << ": " << e.reason << '\n';
        } else if (loaded.holds_error<http::ConfigParseError>()) {
            const auto& e = loaded.error<http::ConfigParseError>();
            std::cerr << "config: parse error at " << e.path << ':' << e.line << ": " << e.detail << '\n';
        } else {
            const auto& e = loaded.error<http::ConfigSchemaError>();
            std::cerr << "config: invalid value in " << e.path << ": " << e.detail << '\n';
        }
        return 1;
    }

}  // namespace

int main(const int argc, char* argv[]) {
    namespace http = demiplane::http;

    http::ServerConfig cfg = http::ServerConfig::Builder{}.finalize();
    if (argc > 1) {
        auto loaded = http::load_server_config(argv[1]);
        if (loaded.is_error()) {
            return report_config_error(loaded);
        }
        cfg = std::move(loaded).value();
    }

    const std::size_t threads = cfg.threads();
    std::cout << "minimal_http_server: " << threads << " io threads — Ctrl+C for graceful shutdown\n";

    try {
        http::run_standalone(std::move(cfg), threads, [](http::Server& server) {
            // JSON-declared listeners (no-op when the config has none) …
            http::attach_default_listeners(server);
            // … falling back to a programmatic one so both paths just work.
            if (server.listeners().empty()) {
                server.add_tcp_listener("127.0.0.1", 8080, http::Http11Driver{http::Http11Config{}});
            }

            auto greeter = std::make_shared<GreeterController>();
            greeter->add_middleware(server_tag_middleware());
            server.in_group("/api").add_controller(std::move(greeter));
        });
    } catch (const std::exception& e) {
        std::cerr << "minimal_http_server: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
