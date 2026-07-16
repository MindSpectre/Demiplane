#include "run_standalone.hpp"

#include <csignal>
#include <deque>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

namespace demiplane::http {

    void run_standalone(ServerConfig cfg, const std::size_t threads, const std::function<void(Server&)>& configure) {
        if (threads == 0) {
            throw std::invalid_argument{"run_standalone: threads must be >= 1"};
        }
        // One io_context PER worker thread (README Finding 13): no shared
        // scheduler queue, no reactor-lock contention between workers —
        // measured +11% over N threads on one context at the raw-asio floor.
        // Connections are distributed across the contexts round-robin by the
        // listeners; contexts.front() is the Server's control executor.
        // DEFAULT concurrency hint on purpose: the {1} single-runner hint
        // measured consistently SLOWER here (README Finding 13) despite the
        // bare probe preferring it — do not "optimize" this to {1}.
        // deque: io_context is immovable; declared BEFORE the Server so the
        // contexts outlive it (§9.6 RAII backstop requirement).
        std::deque<boost::asio::io_context> contexts;
        std::vector<boost::asio::executor_work_guard<Executor>> guards;
        std::vector<Executor> execs;
        guards.reserve(threads);
        execs.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            auto& ctx = contexts.emplace_back();
            // Keeps ctx.run() from returning while its only pending work is
            // the signal wait (and before setup() spawns the accept loops).
            guards.emplace_back(boost::asio::make_work_guard(ctx));
            execs.push_back(ctx.get_executor());
        }

        Server server{std::move(cfg), execs};
        configure(server);  // build phase — single-threaded, nothing running yet

        // Armed only AFTER setup() below: a SIGINT while the server is not yet
        // serving keeps the default disposition (process dies — nothing to
        // drain yet). std::optional so destruction lands AFTER the workers
        // join: signal_set is thread-unsafe as a shared object, so both its
        // async_wait registration (main thread, workers already running — the
        // object is not yet shared then) and its destructor cancel must not
        // race a concurrent delivery. No explicit cancel() is needed either —
        // guards.reset()+stop end the workers regardless of the pending
        // wait, and the destructor cancels single-threaded after the joins.

        // Workers start BEFORE setup(): setup() blocks on the observers'
        // on_setup_complete barrier, which needs a driven control executor.
        // Declared LAST so they join FIRST at scope exit; signals/server/
        // contexts then unwind single-threaded.
        std::optional<boost::asio::signal_set> signals;

        std::vector<std::jthread> workers;
        workers.reserve(threads);
        for (auto& ctx : contexts) {
            workers.emplace_back([&ctx] { ctx.run(); });  // one runner per context
        }

        const auto stop_all = [&] {
            for (auto& g : guards)
                g.reset();
            for (auto& ctx : contexts)
                ctx.stop();
        };

        try {
            server.setup();
            signals.emplace(contexts.front(), SIGINT, SIGTERM);
            signals->async_wait([&server](const boost::system::error_code& ec, int /*signo*/) {
                if (!ec) {
                    server.stop();  // thread-safe, idempotent
                }
            });
        } catch (...) {
            stop_all();  // workers join at scope exit, then signals/server/contexts unwind
            throw;
        }
        gears::unused_value(signals);
        server.wait_until_stopped();  // §9.7: workers keep driving ALL contexts until here

        // NOW it is safe to tear the executors down (§9.7 canonical sequence).
        stop_all();
        // jthread workers join on scope exit; server/contexts are destroyed
        // after them — every coroutine frame already unwound (phase 2.5).
    }

}  // namespace demiplane::http
