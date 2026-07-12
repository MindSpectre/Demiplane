#pragma once

#include <atomic>
#include <concepts>
#include <condition_variable>
#include <cstddef>
#include <demiplane/gears>
#include <demiplane/nexus>
#include <demiplane/scroll>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <controller.hpp>
#include <executor.hpp>
#include <group.hpp>
#include <http_driver_concept.hpp>
#include <listener_base.hpp>
#include <quic_listener.hpp>
#include <route_registry.hpp>
#include <router.hpp>
#include <server_config.hpp>
#include <server_observer.hpp>
#include <tcp_listener.hpp>
#include <tls_config.hpp>
#include <tls_listener.hpp>

namespace demiplane::http {

    /// Thrown by Server::setup() when one or more listeners fail to bind.
    /// setup() attempts EVERY listener first (best effort, so the operator
    /// sees every bad endpoint at once), then clears the listener set — the
    /// acceptors that DID bind close via RAII instead of being left LISTENING
    /// with no accept loop (clients would connect into the kernel backlog and
    /// hang forever).
    class ListenerBindError final : public std::runtime_error {
    public:
        explicit ListenerBindError(std::vector<std::string> failures)
            : std::runtime_error{format_message(failures)},
              failures_{std::move(failures)} {
        }

        [[nodiscard]] const std::vector<std::string>& failures() const noexcept {
            return failures_;
        }

    private:
        [[nodiscard]] static std::string format_message(const std::vector<std::string>& failures);

        std::vector<std::string> failures_;
    };

    /**
     * @brief Thin lifecycle orchestrator over the landed layers (spec §9).
     *
     * The Server is HANDED one or more executors and owns no io_context and
     * no threads (spec §3/§9.1): the caller decides the thread↔context
     * topology, and HTTP coexists with the logger / DB pool / S3 client on
     * caller-owned executors. Connections are distributed round-robin across
     * the injected executors by the listeners (one io_context per worker
     * thread — the throughput topology, README Finding 13); execs.front() is
     * the CONTROL executor: setup barriers, observers, and the shutdown
     * coroutine run there. CONTRACT: each injected executor is driven by AT
     * MOST ONE thread — per-connection serialization is the connection's
     * single-runner home context (Strand is a bare-executor alias, see
     * executor.hpp). Every executor must be driven for the §9.7 contract
     * below. setup() binds synchronously, awaits the observers'
     * on_setup_complete() as a BARRIER (observer-owned resources exist before
     * the first request is served; the control executor must already be
     * driven when observers are registered), then co_spawns each listener's
     * accept loop onto ITS OWN strand (D5 — a stop emit is only safe when
     * serialized with the loop's turns) bound to ITS OWN cancellation signal
     * (a slot holds one handler, spec §7.2). stop() is non-blocking +
     * idempotent and NEVER stops the executors. Shutdown ordering contract
     * (§9.7): after stop(), keep driving ALL executors until
     * wait_until_stopped() / async_wait_stopped() returns — only THEN tear
     * them down. wait_until_stopped() must be called from a thread NOT
     * driving any executor (use async_wait_stopped() there).
     *
     * Build phase (add_* / in_group) is single-threaded by contract; any
     * registration after setup() throws std::logic_error. A correct caller
     * destroys the Server only after wait_until_stopped() returned; the
     * destructor is an RAII backstop that runs the stop→wait sequence itself
     * (see ~Server()).
     */
    class Server : gears::Immutable {
    public:
        NEXUS_REGISTER(nexus::Immortal);

        /// Single-executor convenience — delegates to the vector overload.
        Server(ServerConfig cfg, Executor exec);

        /// One or more executors; connections are distributed round-robin
        /// across them (one io_context per thread is the intended topology).
        /// Throws std::invalid_argument when `execs` is empty.
        Server(ServerConfig cfg, std::vector<Executor> execs);

        /// RAII backstop (spec §9.6): if the Server is destroyed while active,
        /// runs stop() + wait_until_stopped() itself — the typical main()
        /// declares the executor before the Server, so the Server dies first
        /// while the executor is still driven and shutdown completes cleanly.
        /// REQUIRES the executor to be driven by another thread and the
        /// destructor to NOT run on an executor thread — otherwise this blocks
        /// forever (loudly, see the WRN log) rather than freeing members under
        /// still-live coroutine frames (use-after-free).
        ~Server();

        // ── Build phase ───────────────────────────────────────────────────
        template <IsHttpDriver Driver>
        Server& add_tcp_listener(std::string host, const std::uint16_t port, Driver driver) {
            require_build("add_tcp_listener");
            listeners_.push_back(std::make_unique<TcpListener<Driver>>(
                execs_, std::move(host), port, std::move(driver), cfg_.request_arena_size()));
            return *this;
        }

        template <IsHttpDriver... Drivers>
        Server& add_tls_listener(std::string host, const std::uint16_t port, TlsConfig tls, Drivers... drivers) {
            require_build("add_tls_listener");
            listeners_.push_back(std::make_unique<TlsListener<Drivers...>>(
                execs_, std::move(host), port, std::move(tls), cfg_.request_arena_size(), std::move(drivers)...));
            return *this;
        }

        template <IsHttpDriver Driver>
        Server& add_quic_listener(std::string host, const std::uint16_t port, TlsConfig tls, Driver driver) {
            require_build("add_quic_listener");
            listeners_.push_back(std::make_unique<QuicListener<Driver>>(
                execs_, std::move(host), port, std::move(tls), std::move(driver)));
            return *this;
        }

        template <std::derived_from<HttpController> C>
        Server& add_controller(std::shared_ptr<C> ctrl) {
            in_group("").add_controller(std::move(ctrl));
            return *this;
        }

        /// Prefix-scoped mounting (spec §8.4). The returned binding writes into
        /// this Server's registry/controller list; using it after setup()
        /// throws via the frozen registry.
        [[nodiscard]] GroupBinding in_group(std::string prefix) {
            require_build("in_group");
            return GroupBinding{registry_, controllers_, std::move(prefix)};
        }

        Server& add_observer(std::shared_ptr<ServerObserver> obs);

        // ── Lifecycle ─────────────────────────────────────────────────────
        /// Freeze routes (all conflicts thrown at once), bind every listener
        /// (best-effort-all, failures aggregated), await the observers'
        /// on_setup_complete() barrier, spawn accept loops. Spawns no threads;
        /// blocks ONLY on the observer barrier — with observers registered the
        /// injected executor must already be driven by another thread, and
        /// setup() must not be called from an executor thread.
        /// Throws: std::logic_error (state / no listeners),
        /// RouteConflictAggregateError, ListenerBindError. On throw the Server
        /// is NOT running and must be discarded.
        void setup();

        /// Request graceful shutdown; non-blocking, idempotent, thread-safe.
        /// Callable from executor threads (signal handlers, request handlers).
        /// stop() before setup() is a documented no-op; a stop() racing
        /// setup() is LATCHED and honored the moment setup() finishes.
        /// NEVER stops the executor (§9.7). CAVEAT: the caller must ensure the
        /// injected executor outlives the stop() CALL itself — stop() posts
        /// into it, and the post's scheduler-signal tail races an executor
        /// teardown that begins the instant shutdown completes. In
        /// run_standalone mode (internal executor) request shutdown via
        /// SIGINT/SIGTERM or from within a handler/observer, never from an
        /// external thread.
        void stop();

        /// Block until graceful shutdown completes (§9.7). Returns immediately
        /// if setup() never ran. MUST NOT be called from an executor thread —
        /// it would block the very shutdown it waits for; use
        /// async_wait_stopped() there.
        void wait_until_stopped();

        /// Awaitable twin for callers already running on the injected
        /// executor. Polls with exponential backoff (5ms → 320ms cap): the cv
        /// stays the ONLY internal completion primitive — a second one would
        /// re-create the phase-5 last-touch race — and the polling coroutine
        /// is caller-owned, so its lifetime follows ordinary object rules.
        [[nodiscard]] boost::asio::awaitable<void> async_wait_stopped() const;

        // ── Introspection ─────────────────────────────────────────────────
        [[nodiscard]] bool is_running() const noexcept {
            return state_.load(std::memory_order_acquire) == State::running;
        }
        [[nodiscard]] std::span<const std::unique_ptr<ListenerBase>> listeners() const noexcept {
            return listeners_;
        }
        [[nodiscard]] const ServerConfig& config() const noexcept {
            return cfg_;
        }

    private:
        /// `starting` covers setup()'s live window (observer barrier + loop
        /// spawn): a stop() arriving there latches stop_requested_ instead of
        /// silently no-oping, and setup()'s tail honors it (no lost stops, no
        /// "serving but !is_running()" observation artifacts beyond the spawn
        /// instant itself).
        enum class State : std::uint8_t { build, starting, running, stopping, stopped };

        void require_build(std::string_view what) const;
        void begin_shutdown();
        void wire_observer_hooks();
        void fan_unhandled_exception(const std::exception_ptr& ep) const noexcept;
        [[nodiscard]] std::size_t total_in_flight() const noexcept;
        boost::asio::awaitable<void> notify_setup_observers() const;
        boost::asio::awaitable<void> graceful_shutdown();
        [[nodiscard]] static PathNormalization map_normalization(ServerConfig::PathNormalization n) noexcept;

        ServerConfig cfg_;
        // Injected; NOT owned, never stopped. Non-empty (ctor-validated).
        // front() is the control executor (setup barrier, observers, the
        // shutdown coroutine); listeners round-robin connections over all.
        std::vector<Executor> execs_;
        std::atomic<State> state_{State::build};

        RouteRegistry registry_;
        Router router_{registry_};
        std::vector<std::shared_ptr<HttpController>> controllers_;
        std::vector<std::shared_ptr<ServerObserver>> observers_;
        std::vector<std::unique_ptr<ListenerBase>> listeners_;

        // One strand + one stop signal PER listener (spec §7.2/§9.3: a slot
        // holds a single handler; the emit must be serialized with the accept
        // loop's turns — D5). cancellation_signal is immovable; shared_ptr
        // because the phase-1 emit lambdas are queued in io_context-owned
        // strand queues and can outlive the Server — each keeps its signal
        // alive, so a late emit fires on a live-but-orphaned signal harmlessly.
        std::vector<Strand> run_strands_;
        std::vector<std::shared_ptr<boost::asio::cancellation_signal>> stop_signals_;
        std::atomic<std::size_t> live_accept_loops_{0};
        std::atomic<bool> stop_requested_{false};  // stop() during State::starting

        std::mutex shutdown_mutex_;
        std::condition_variable shutdown_cv_;
        bool shutdown_complete_ = false;

        SCROLL_COMPONENT_PREFIX("Server");
    };

}  // namespace demiplane::http
