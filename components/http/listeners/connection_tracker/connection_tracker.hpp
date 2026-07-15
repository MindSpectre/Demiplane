#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <demiplane/gears>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <utility>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/dispatch.hpp>
#include <executor.hpp>
namespace demiplane::http {

    /**
     * @brief Tracks in-flight connections for graceful shutdown AND per-phase
     * deadline enforcement (spec §7.2, D2; README Finding 13).
     *
     * The landed connections own their own cancellation_signal (the driver binds
     * I/O to conn.cancel_slot()), so this tracker does NOT own signals. Instead
     * each entry holds a weak_ptr to the connection + a thunk that dispatches
     * `conn->cancel()` (emit terminal on the connection's own signal) onto the
     * connection's executor. drain_until() polls the counter and, at the
     * deadline, force-cancels every survivor; the weak_ptr makes a late
     * force-cancel use-after-free-safe.
     *
     * Deadline enforcement: ONE sweep coroutine per listener (start_sweep)
     * walks the entries every `tick` and force-cancels connections whose
     * deadline() passed. This replaces the per-connection watchdog timer of the
     * first design: per-connection timers mean O(connections) timer wakeups per
     * tick, which measurably stalls single-runner (io_context-per-thread)
     * workers — 256 conns at a 500ms tick cost 35% of pipeline-1 throughput.
     * The sweep costs 2 timer ops/second TOTAL, on the listener's home
     * executor, and reads each connection's deadline through an atomic.
     *
     * register_connection() requires the concrete connection type to have
     * `void cancel()` and a thread-safe `deadline()` (TcpConnection /
     * TlsConnection: atomic load) — neither is on the IsConnection concept.
     *
     * Internals live in a shared_ptr'd State: the sweep coroutine holds a
     * weak_ptr, so a listener (and tracker) destroyed while the executors are
     * still running never leaves the sweep dangling — it exits on its next
     * tick when the weak_ptr fails to lock (or the dtor's stop flag is seen).
     */
    class ConnectionTracker : gears::Immutable {
    public:
        ConnectionTracker() = default;

        ~ConnectionTracker() {
            state_->stop.store(true, std::memory_order_release);
        }

        struct Entry {
            std::weak_ptr<void> conn;
            std::function<void(const std::shared_ptr<void>&)> cancel;
            // Stateless accessor (plain function pointer, no allocation):
            // reads the connection's atomic deadline for the sweep.
            std::chrono::steady_clock::time_point (*deadline)(const std::shared_ptr<void>&);
        };

    private:
        struct State {
            std::atomic<std::size_t> in_flight{0};
            std::atomic<bool> stop{false};
            std::mutex mu;
            // TODO(C++26): replace std::list with std::hive (P0447) once libc++ ships it.
            // Handle stores an iterator into this container and erases by it in release(),
            // so we need iterator/reference stability across *other* connections'
            // insert/erase. std::list gives that but at the cost of a heap node per
            // connection and poor cache locality in the sweep/drain full scans.
            std::list<Entry> entries;
        };

    public:
        /// RAII deregistration: on destruction, erase the entry + decrement the
        /// counter. Move-only (move nulls the source so the dtor is a no-op).
        /// Holds the State shared_ptr — release is safe even past the owning
        /// listener's death.
        class Handle : gears::NonCopyable {
        public:
            template <typename StateSharedPtrTp, typename ListIteratorTp>
                requires std::is_same_v<std::remove_cvref_t<StateSharedPtrTp>, std::shared_ptr<State>> &&
                             std::is_same_v<std::remove_cvref_t<ListIteratorTp>, std::list<Entry>::iterator>
            Handle(StateSharedPtrTp&& state, ListIteratorTp&& it) noexcept
                : state_{std::forward<StateSharedPtrTp>(state)},
                  it_{std::forward<ListIteratorTp>(it)} {
            }
            Handle(Handle&& o) noexcept
                : state_{std::move(o.state_)},
                  it_{o.it_} {
                o.state_ = nullptr;
            }
            Handle& operator=(Handle&& o) noexcept {
                if (this != &o) {
                    release();
                    state_   = std::move(o.state_);
                    it_      = o.it_;
                    o.state_ = nullptr;
                }
                return *this;
            }

            ~Handle() {
                release();
            }

        private:
            void release() noexcept;
            std::shared_ptr<State> state_;
            std::list<Entry>::iterator it_;
        };

        /// `ex` is the CONNECTION's executor — cancel() must be serialized with
        /// that connection's in-flight I/O (its single-runner context / strand).
        template <typename Conn>
        Handle register_connection(const std::shared_ptr<Conn>& conn, Strand ex) {
            auto thunk = [ex = std::move(ex)](const std::shared_ptr<void>& c) {
                boost::asio::dispatch(ex, [c] { std::static_pointer_cast<Conn>(c)->cancel(); });
            };
            constexpr auto deadline_of =
                +[](const std::shared_ptr<void>& c) { return std::static_pointer_cast<Conn>(c)->deadline(); };
            std::lock_guard lk{state_->mu};
            state_->in_flight.fetch_add(1, std::memory_order_acq_rel);
            const auto it = state_->entries.insert(state_->entries.end(),
                                                   Entry{std::weak_ptr<void>{conn}, std::move(thunk), deadline_of});
            return Handle{state_, it};
        }

        /// Spawn the deadline sweep (idempotent; call at accept-loop start).
        /// Runs detached on `ex` until the tracker dies or `stop` is set.
        void start_sweep(const Executor& ex, std::chrono::milliseconds tick = std::chrono::milliseconds{500});

        /// Poll the counter until it reaches 0 or `deadline` passes, then
        /// force-cancel every surviving connection. Runs on `ex`.
        /// `ex` BY VALUE, never by reference: this is a COROUTINE — its frame
        /// stores reference parameters as references, so a caller's temporary
        /// executor dies at the first suspension (ASan: stack-use-after-scope
        /// on resume). Executor is a cheap handle; the frame must own a copy.
        [[nodiscard]] boost::asio::awaitable<void> drain_until(Executor ex,
                                                               std::chrono::steady_clock::time_point deadline) const;

        [[nodiscard]] std::size_t in_flight() const noexcept {
            return state_->in_flight.load(std::memory_order_acquire);
        }

    private:
        // `ex` by value — coroutine, same dangling-reference rule as drain_until.
        [[nodiscard]] static boost::asio::awaitable<void>
        sweep(std::weak_ptr<State> weak, Executor ex, std::chrono::milliseconds tick);

        std::shared_ptr<State> state_ = std::make_shared<State>();
        bool sweep_started_           = false;  // build/run transition is single-threaded
    };

}  // namespace demiplane::http
