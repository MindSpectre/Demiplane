#include "connection_tracker.hpp"

#include <vector>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace demiplane::http {

    void ConnectionTracker::Handle::release() noexcept {
        if (state_ == nullptr) {
            return;
        }
        {
            std::lock_guard lk{state_->mu};
            state_->entries.erase(it_);
        }
        state_->in_flight.fetch_sub(1, std::memory_order_acq_rel);
        state_ = nullptr;
    }

    void ConnectionTracker::start_sweep(const Executor& ex, const std::chrono::milliseconds tick) {
        if (sweep_started_) {
            return;
        }
        sweep_started_ = true;
        boost::asio::co_spawn(ex, sweep(state_, ex, tick), boost::asio::detached);
    }

    boost::asio::awaitable<void> ConnectionTracker::sweep(const std::weak_ptr<State> weak,
                                                          const Executor ex,  // by value — coroutine (see header)
                                                          const std::chrono::milliseconds tick) {
        boost::asio::steady_timer timer{ex};
        boost::system::error_code ec;  // wake errors are exit signals, not failures
        for (;;) {
            timer.expires_after(tick);
            co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            const auto state = weak.lock();
            if (!state || state->stop.load(std::memory_order_acquire) || ec) {
                co_return;  // tracker gone / listener tearing down / executor stopping
            }
            const auto now = std::chrono::steady_clock::now();
            // Copy the expired entries' thunks under the lock; fire after
            // releasing it (a thunk dispatches onto the connection's executor,
            // and a completing connection's Handle dtor also takes the lock —
            // same discipline as drain_until below).
            std::vector<std::pair<std::shared_ptr<void>, std::function<void(const std::shared_ptr<void>&)>>> expired;
            {
                std::lock_guard lk{state->mu};
                for (const auto& [conn_weak, cancel_func, deadline] : state->entries) {
                    if (auto conn = conn_weak.lock(); conn && deadline(conn) <= now) [[unlikely]] {
                        expired.emplace_back(std::move(conn), cancel_func);
                    }
                }
            }
            for (const auto& [conn, cancel] : expired) {
                cancel(conn);  // level-triggered kill; idempotent per connection
            }
        }
    }

    boost::asio::awaitable<void>  // ex by value — coroutine (see header)
    ConnectionTracker::drain_until(const Executor ex, const std::chrono::steady_clock::time_point deadline) const {
        using namespace std::chrono_literals;
        boost::asio::steady_timer timer{ex};

        while (state_->in_flight.load(std::memory_order_acquire) > 0 && std::chrono::steady_clock::now() < deadline) {
            timer.expires_after(20ms);
            co_await timer.async_wait(boost::asio::use_awaitable);
        }

        // Snapshot the survivors under the lock; fire the cancel thunks after
        // releasing it (a thunk dispatches onto another executor, and a completing
        // connection's Handle dtor also takes the lock — do not hold it across).
        std::vector<Entry> survivors;
        {
            std::lock_guard lk{state_->mu};
            survivors.reserve(state_->entries.size());
            for (const auto& e : state_->entries) {
                survivors.push_back(e);
            }
        }
        for (const auto& e : survivors) {
            if (auto conn = e.conn.lock()) {  // skip connections that already finished
                e.cancel(conn);               // `conn` kept alive across the dispatch by capture
            }
        }
        // WARN: this only DISPATCHES force-cancels; it does not wait for the cancelled serve() coroutines
        // to unwind (in_flight -> 0) — on ANY executor: the cancels and the unwinds run in later executor turns, so
        // drain returning never implies the frames are gone. Callers must wait for in_flight() == 0 before destroying
        // the owning listener (Server::graceful_shutdown phase 2.5 polls it; the integration fixture polls in
        // TearDown).
        co_return;
    }

}  // namespace demiplane::http
