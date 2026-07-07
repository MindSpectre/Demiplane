#pragma once

#include <cstddef>
#include <functional>

#include <server.hpp>
#include <server_config.hpp>

namespace demiplane::http {

    /**
     * @brief Convenience for the "HTTP owns the process" case (spec §9.1).
     *
     * Creates an internal io_context + `threads` worker threads, calls
     * `configure(server)` for listener/controller/observer wiring, runs
     * setup(), installs SIGINT/SIGTERM → stop(), blocks until graceful
     * shutdown completes, then tears the context down and joins — exactly the
     * §9.7 stop → wait → stop-context → join sequence, so trivial apps never
     * reason about the shutdown-ordering contract.
     *
     * Throws std::invalid_argument on threads == 0; configure()/setup()
     * exceptions propagate (the internal context is stopped and the workers
     * joined before the rethrow).
     *
     * SHUTDOWN TRIGGERS: SIGINT/SIGTERM, or Server::stop() called from WITHIN
     * a handler/observer (i.e. on the internal executor). An external thread
     * must NOT call stop() on a run_standalone-owned Server: the internal
     * io_context is destroyed the moment shutdown completes, racing the
     * external stop()'s post tail (see Server::stop() caveat).
     */
    void run_standalone(ServerConfig cfg, std::size_t threads, const std::function<void(Server&)>& configure);

}  // namespace demiplane::http
