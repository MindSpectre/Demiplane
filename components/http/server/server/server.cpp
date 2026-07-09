#include "server.hpp"

#include <algorithm>
#include <chrono>
#include <demiplane/chrono>
#include <exception>
#include <stdexcept>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>

namespace demiplane::http {

    namespace {
        using namespace std::chrono_literals;
        // Poll cadence for the accept-loop / in-flight unwind barriers (D6).
        // Force-cancelled frames unwind within a few executor turns, so the
        // shutdown polls are short in practice; async_wait_stopped backs off
        // from POLL_TICK up to MAX_POLL_TICK.
        constexpr auto POLL_TICK     = 5ms;
        constexpr auto MAX_POLL_TICK = 320ms;
    }  // namespace

    std::string ListenerBindError::format_message(const std::vector<std::string>& failures) {
        std::string msg = std::to_string(failures.size()) + " listener bind failure(s):";
        for (const auto& f : failures) {
            msg += "\n  ";
            msg += f;
        }
        return msg;
    }

    Server::Server(ServerConfig cfg, boost::asio::any_io_executor exec)
        : cfg_{std::move(cfg)},
          exec_{std::move(exec)},
          // Read the MEMBER, not the moved-from parameter (D8): members
          // initialize in declaration order, so cfg_ is populated by now.
          registry_{map_normalization(cfg_.path_normalization())} {
    }

    Server::~Server() {
        if (const auto s = state_.load(std::memory_order_acquire); s == State::build || s == State::stopped) {
            return;  // nothing is running — members unwind via plain RAII
        }
        // RAII backstop: the §9.7 stop→wait sequence is the caller's job, but
        // a Server dying first in main() (declared after the executor, so
        // destroyed before it) still shuts down cleanly here as long as the
        // executor is being driven. If it is not — or this destructor runs ON
        // an executor thread — this blocks forever, loudly, instead of freeing
        // members under still-live coroutine frames (use-after-free).
        COMPONENT_LOG_WRN() << "Server destroyed while active — running stop() + wait_until_stopped() as an "
                               "RAII backstop (§9.7 sequencing is the caller's responsibility)";
        stop();
        wait_until_stopped();
    }

    Server& Server::add_observer(std::shared_ptr<ServerObserver> obs) {
        require_build("add_observer");
        if (!obs) {
            throw std::invalid_argument{"Server::add_observer: null observer"};
        }
        observers_.push_back(std::move(obs));
        return *this;
    }

    void Server::setup() {
        require_build("setup");
        if (listeners_.empty()) {
            throw std::logic_error{"Server::setup: no listeners added"};
        }
        if (auto conflicts = registry_.freeze(); !conflicts.empty()) {
            throw RouteConflictAggregateError{std::move(conflicts)};
        }
        // Bind is best-effort-all-then-report: every listener gets its attempt
        // so the operator sees EVERY bad endpoint at once; on any failure the
        // whole set is cleared — acceptors that DID bind close via RAII
        // instead of sitting LISTENING with no accept loop (clients would
        // connect into the kernel backlog and hang). Controllers need no
        // symmetric unwind: they are RAII (ctor acquires, dtor releases).
        {
            std::vector<std::string> bind_failures;
            for (const auto& listener : listeners_) {
                try {
                    listener->bind();
                } catch (const std::exception& e) {
                    bind_failures.push_back(listener->bind_address() + " — " + e.what());
                }
            }
            if (!bind_failures.empty()) {
                listeners_.clear();
                throw ListenerBindError{std::move(bind_failures)};
            }
        }
        wire_observer_hooks();

        // From here stop() may legitimately race setup() — an observer's
        // on_setup_complete (or, once the loops spawn, an early request
        // handler) can request shutdown. `starting` makes such a stop() LATCH
        // (stop_requested_) instead of silently no-oping; the tail re-checks.
        state_.store(State::starting, std::memory_order_release);

        if (!observers_.empty()) {
            // Barrier: the observers' on_setup_complete() finishes BEFORE the
            // accept loops spawn, so observer-owned resources exist before the
            // first request is served. bind() already listen()s — connections
            // arriving meanwhile queue in the kernel backlog: delayed, never
            // lost. REQUIRES the injected executor to be driven by some OTHER
            // thread; per-observer throws are fanned to on_unhandled_exception
            // inside notify_setup_observers().
            boost::asio::co_spawn(exec_, notify_setup_observers(), boost::asio::use_future).get();
        }

        run_strands_.reserve(listeners_.size());
        stop_signals_.reserve(listeners_.size());
        live_accept_loops_.store(listeners_.size(), std::memory_order_release);
        for (const auto& listener : listeners_) {
            run_strands_.emplace_back(boost::asio::make_strand(exec_));
            stop_signals_.emplace_back(std::make_shared<boost::asio::cancellation_signal>());
            boost::asio::co_spawn(
                run_strands_.back(),
                listener->run(router_),
                boost::asio::bind_cancellation_slot(stop_signals_.back()->slot(), [this](const std::exception_ptr& ep) {
                    // Runs on the listener's strand when run() finishes.
                    // run() treats cancellation as state, so ep is an
                    // ESCAPE (accept-loop bug/fatal) — surface it.
                    // TODO: teardown policy — an accept loop dying while the
                    //  server keeps running only notifies observers today;
                    //  consider a configurable "all listeners dead → stop()"
                    //  (or restart) policy.
                    if (ep) {
                        fan_unhandled_exception(ep);
                    }
                    live_accept_loops_.fetch_sub(1, std::memory_order_acq_rel);
                }));
        }
        state_.store(State::running, std::memory_order_release);
        COMPONENT_LOG_INF() << "setup complete: " << listeners_.size() << " listener(s) live";
        if (stop_requested_.exchange(false, std::memory_order_acq_rel)) {
            stop();  // honor a stop() that raced setup() instead of losing it
        }
    }

    void Server::stop() {
        auto expected = State::running;
        if (state_.compare_exchange_strong(expected, State::stopping, std::memory_order_acq_rel)) {
            begin_shutdown();
            return;
        }
        if (expected != State::starting) {
            return;  // build (stop-before-setup no-op), stopping, stopped: idempotent
        }
        // setup() is mid-flight (observer barrier / loop spawn). Latch the
        // request — setup()'s tail honors it after storing `running`. Re-check
        // ourselves too: setup() may have flipped to `running` (and already
        // consumed the flag) between our first CAS and the latch.
        stop_requested_.store(true, std::memory_order_release);
        expected = State::running;
        if (state_.compare_exchange_strong(expected, State::stopping, std::memory_order_acq_rel)) {
            stop_requested_.store(false, std::memory_order_relaxed);
            begin_shutdown();
        }
    }

    void Server::begin_shutdown() {
        COMPONENT_LOG_INF() << "stop requested — spawning graceful shutdown";
        boost::asio::co_spawn(exec_, graceful_shutdown(), boost::asio::detached);
    }

    void Server::wait_until_stopped() {
        std::unique_lock lk{shutdown_mutex_};
        shutdown_cv_.wait(
            lk, [this] { return shutdown_complete_ || state_.load(std::memory_order_acquire) == State::build; });
    }

    boost::asio::awaitable<void> Server::async_wait_stopped() const {
        // Poll with exponential backoff instead of a Server-side event: the cv
        // stays the ONLY internal completion primitive (a second one would
        // re-create the phase-5 last-touch race), and this coroutine is
        // caller-owned, so its lifetime follows ordinary object rules.
        // Steady-state cost ≈3 wakeups/s; shutdown-detection latency is
        // bounded by MAX_POLL_TICK.
        std::chrono::milliseconds delay = POLL_TICK;
        while (true) {
            if (const auto s = state_.load(std::memory_order_acquire); s == State::stopped || s == State::build) {
                co_return;
            }
            co_await demiplane::chrono::async_sleep_for(exec_, delay);
            delay = std::min<std::chrono::milliseconds>(delay * 2, MAX_POLL_TICK);
        }
    }

    boost::asio::awaitable<void> Server::graceful_shutdown() {
        namespace asio = boost::asio;

        // Phase 1: cancel accept loops — one signal per listener, each emit
        // DISPATCHED onto the strand its run() executes on (D5: emit is only
        // safe when serialized with the loop's turns on a multi-threaded
        // executor; this also closes the edge-lost-emit window). The lambda is
        // queued in an io_context-owned strand queue and can outlive the
        // Server (dispatch from off-strand always enqueues, and this coroutine
        // can reach phase 5 without yielding when the loops are already dead)
        // — so it OWNS its signal via shared_ptr: a late emit fires on a
        // live-but-orphaned signal, harmlessly.
        for (std::size_t i = 0; i < listeners_.size(); ++i) {
            asio::dispatch(run_strands_[i], [sig = stop_signals_[i]] { sig->emit(asio::cancellation_type::terminal); });
        }
        try {
            // Phase 1.5: await accept-loop completion. run() closes its acceptor
            // on every exit path, so from here new connections are provably
            // REFUSED, not backlogged (spec §14.2).
            while (live_accept_loops_.load(std::memory_order_acquire) > 0) {
                co_await demiplane::chrono::async_sleep_for(exec_, POLL_TICK);
            }

            // Phase 2: drain in-flight requests up to drain_timeout (shared
            // deadline — total wait is bounded by ONE timeout, not one per
            // listener). At the deadline the trackers force-cancel survivors.
            const auto deadline = std::chrono::steady_clock::now() + cfg_.drain_timeout();
            for (const auto& listener : listeners_) {
                co_await listener->drain_until(deadline);
            }
            // Phase 2.5: unwind barrier (D6). drain_until only DISPATCHES
            // force-cancels; the cancelled serve() frames unwind in later executor
            // turns. Destroying listeners (or letting the caller kill the
            // executor) with frames still suspended would be a use-after-free —
            // poll until every tracker Handle released. Deliberately UNBOUNDED: a
            // suspended frame cannot be freed except by completion, so a handler
            // that ignores cancellation delays shutdown rather than corrupting it.
            if (total_in_flight() > 0) {
                COMPONENT_LOG_WRN() << "drain deadline passed with " << total_in_flight()
                                    << " connection(s) force-cancelled — waiting for unwind";
            }
            while (total_in_flight() > 0) {
                co_await demiplane::chrono::async_sleep_for(exec_, POLL_TICK);
            }

            // Phase 3: async shutdown observers — awaited ON the still-driven
            // executor (§9.7), so real async work finishes before completion is
            // reported. A throwing observer fans to everyone's
            // on_unhandled_exception and shutdown continues. (Controllers have
            // no shutdown hook — they are RAII and unwind with the Server.)
            for (const auto& obs : observers_) {
                try {
                    co_await obs->on_shutdown_started();
                } catch (...) {
                    fan_unhandled_exception(std::current_exception());
                }
            }

            // Phase 4: sync, noexcept completion notifications.
            for (const auto& obs : observers_) {
                obs->on_shutdown_complete();
            }
        } catch (...) {
            // A throw escaping phase 1.5/2/2.5 (e.g. an async_sleep_for or
            // drain_until await completing with an error) must NOT strand this
            // detached coroutine before phase 5 — that would block
            // wait_until_stopped()/run_standalone forever. Fan it out and fall
            // through to latch completion unconditionally below.
            fan_unhandled_exception(std::current_exception());
        }

        // Phase 5: report completion — unblocks wait_until_stopped(). The
        // executor is NOT stopped or drained; the caller owns it (§9.7).
        // ALWAYS runs, even if an earlier phase threw (see catch above).
        COMPONENT_LOG_INF() << "graceful shutdown complete";
        {
            // Latch + notify UNDER the lock: a §9.7-compliant caller may
            // destroy the Server the moment it can observe shutdown_complete_
            // — a caller arriving at wait_until_stopped() between an unlocked
            // latch and the notify would sail through and free the cv under
            // us. With the notify inside the critical section, the earliest
            // that observation can happen is after this coroutine's final
            // unlock, making this block the LAST touch of *this*. (The
            // woken-waiter mutex bounce is a one-off, once per lifetime.)
            std::lock_guard lk{shutdown_mutex_};
            shutdown_complete_ = true;
            state_.store(State::stopped, std::memory_order_release);
            shutdown_cv_.notify_all();
        }
    }

    void Server::require_build(const std::string_view what) const {
        if (state_.load(std::memory_order_acquire) != State::build) {
            throw std::logic_error{"Server::" + std::string{what} +
                                   ": registration/setup after setup() (registry frozen, spec §8.1 phase 3)"};
        }
    }

    void Server::wire_observer_hooks() {
        if (observers_.empty()) {
            return;  // hot path keeps null hooks — one branch, zero fan-out
        }
        // Fan-out lambdas capture only `this` (std::function SBO — invocation
        // is allocation-free, gated in the routing gate test). observers_ is
        // immutable from setup() on (require_build guards add_observer).
        router_.set_hooks(Router::Hooks{
            .on_request =
                [this](const RequestContext& ctx) noexcept {
                    for (const auto& obs : observers_) {
                        obs->on_request(ctx);
                    }
                },
            .on_response =
                [this](const RequestInfo& info, const Response& resp) noexcept {
                    for (const auto& obs : observers_) {
                        obs->on_response(info, resp);
                    }
                },
            .on_unhandled_exception = [this](const std::exception_ptr& ep) noexcept { fan_unhandled_exception(ep); },
        });
    }

    void Server::fan_unhandled_exception(const std::exception_ptr& ep) const noexcept {
        for (const auto& obs : observers_) {
            obs->on_unhandled_exception(ep);
        }
    }

    std::size_t Server::total_in_flight() const noexcept {
        std::size_t n = 0;
        for (const auto& listener : listeners_) {
            n += listener->in_flight();
        }
        return n;
    }

    boost::asio::awaitable<void> Server::notify_setup_observers() const {
        // Awaited by setup() as a BARRIER: sequential, add order, on the
        // injected executor. Per-observer throws are contained here so one
        // failing observer neither aborts setup nor starves its successors.
        for (const auto& obs : observers_) {
            try {
                co_await obs->on_setup_complete();
            } catch (...) {
                fan_unhandled_exception(std::current_exception());
            }
        }
    }

    PathNormalization Server::map_normalization(const ServerConfig::PathNormalization n) noexcept {
        switch (n) {
            case ServerConfig::PathNormalization::none:
                return PathNormalization::none;
            case ServerConfig::PathNormalization::collapse_multi_slash:
                return PathNormalization::collapse_multi_slash;
            case ServerConfig::PathNormalization::collapse_trailing_slash:
                break;
        }
        return PathNormalization::collapse_trailing_slash;
    }

}  // namespace demiplane::http
