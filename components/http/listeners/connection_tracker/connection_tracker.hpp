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
     * @brief Tracks in-flight connections for graceful shutdown (spec §7.2, D2).
     *
     * The landed connections own their own cancellation_signal (the driver binds
     * I/O to conn.cancel_slot()), so this tracker does NOT own signals. Instead
     * each entry holds a weak_ptr to the connection + a thunk that dispatches
     * `conn->cancel()` (emit terminal on the connection's own signal) onto the
     * connection's strand. drain_until() polls the counter and, at the deadline,
     * force-cancels every survivor; the weak_ptr makes a late force-cancel
     * use-after-free-safe (a connection whose serve() already finished is gone).
     *
     * register_connection() requires only that the concrete connection type has
     * a `void cancel()` method (TcpConnection/TlsConnection provide it) — cancel
     * is intentionally not on the IsConnection concept.
     */
    class ConnectionTracker : gears::Immutable {
    public:
        ConnectionTracker() = default;

        struct Entry {
            std::weak_ptr<void> conn;
            std::function<void(const std::shared_ptr<void>&)> cancel;
        };

        /// RAII deregistration: on destruction, erase the entry + decrement the
        /// counter. Move-only (move nulls the source so the dtor is a no-op).
        class Handle : gears::NonCopyable {
        public:
            Handle(ConnectionTracker* tracker, const std::list<Entry>::iterator it) noexcept
                : tracker_{tracker},
                  it_{it} {
            }
            Handle(Handle&& o) noexcept
                : tracker_{o.tracker_},
                  it_{o.it_} {
                o.tracker_ = nullptr;
            }
            Handle& operator=(Handle&& o) noexcept {
                if (this != &o) {
                    release();
                    tracker_   = o.tracker_;
                    it_        = o.it_;
                    o.tracker_ = nullptr;
                }
                return *this;
            }

            ~Handle() {
                release();
            }

        private:
            void release() noexcept;
            ConnectionTracker* tracker_;
            std::list<Entry>::iterator it_;
        };

        /// `strand` is the CONNECTION's strand, not the bare executor — cancel()
        /// must be serialized with that connection's in-flight I/O.
        template <typename Conn>
        Handle register_connection(const std::shared_ptr<Conn>& conn, Strand strand) {
            auto thunk = [strand = std::move(strand)](const std::shared_ptr<void>& c) {
                boost::asio::dispatch(strand, [c] { std::static_pointer_cast<Conn>(c)->cancel(); });
            };
            std::lock_guard lk{mu_};
            in_flight_.fetch_add(1, std::memory_order_acq_rel);
            const auto it = entries_.insert(entries_.end(), Entry{std::weak_ptr<void>{conn}, std::move(thunk)});
            return Handle{this, it};
        }

        /// Poll the counter until it reaches 0 or `deadline` passes, then
        /// force-cancel every surviving connection. Runs on `ex`.
        boost::asio::awaitable<void> drain_until(Executor ex, std::chrono::steady_clock::time_point deadline);

        [[nodiscard]] std::size_t in_flight() const noexcept {
            return in_flight_.load(std::memory_order_acquire);
        }

    private:
        std::atomic<std::size_t> in_flight_{0};
        std::mutex mu_;
        // TODO(C++26): replace std::list with std::hive (P0447) once libc++ ships it.
        // Handle stores an iterator into this container and erases by it in release(),
        // so we need iterator/reference stability across *other* connections'
        // insert/erase. std::list gives that but at the cost of a heap node per
        // connection and poor cache locality in drain_until's full scan. std::hive
        // offers the same stability (only the erased element's iterator is
        // invalidated) with block-contiguous storage -> better locality, no per-node
        // allocation, O(1) erase-by-iterator. Entry order is irrelevant here.
        std::list<Entry> entries_;
    };

}  // namespace demiplane::http
