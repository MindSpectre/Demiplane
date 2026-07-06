#include "run_standalone.hpp"

#include <csignal>
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
        boost::asio::io_context ioc{static_cast<int>(threads)};
        // Keeps ioc.run() from returning while the only pending work is the
        // signal wait (and before setup() spawns the accept loops).
        auto guard = boost::asio::make_work_guard(ioc);

        Server server{std::move(cfg), ioc.get_executor()};
        configure(server);  // build phase — single-threaded, nothing on ioc yet

        // Armed only AFTER setup() below: a SIGINT while the server is not yet
        // serving keeps the default disposition (process dies — nothing to
        // drain yet). std::optional so destruction lands AFTER the worker
        // joins: signal_set is thread-unsafe as a shared object, so both its
        // async_wait registration (main thread, workers already running — the
        // object is not yet shared then) and its destructor cancel must not
        // race a concurrent delivery. No explicit cancel() is needed either —
        // guard.reset()+ioc.stop() end the workers regardless of the pending
        // wait, and the destructor cancels single-threaded after the joins.

        // Workers start BEFORE setup(): setup() blocks on the observers'
        // on_setup_complete barrier, which needs a driven executor. Declared
        // LAST so they join FIRST at scope exit; signals/server/ioc then
        // unwind single-threaded.
        std::optional<boost::asio::signal_set> signals;

        std::vector<std::jthread> workers;
        workers.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            workers.emplace_back([&ioc] { ioc.run(); });
        }

        try {
            server.setup();
            signals.emplace(ioc, SIGINT, SIGTERM);
            signals->async_wait([&server](const boost::system::error_code& ec, int /*signo*/) {
                if (!ec) {
                    server.stop();  // thread-safe, idempotent
                }
            });
        } catch (...) {
            guard.reset();
            ioc.stop();  // workers join at scope exit, then signals/server/ioc unwind
            throw;
        }
        gears::unused_value(signals);
        server.wait_until_stopped();  // §9.7: workers keep driving ioc until here

        // NOW it is safe to tear the executor down (§9.7 canonical sequence).
        guard.reset();
        ioc.stop();
        // jthread workers join on scope exit; server/ioc are destroyed after
        // them — every coroutine frame already unwound (phase 2.5).
    }

}  // namespace demiplane::http
