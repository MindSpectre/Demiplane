#include "connection_tracker.hpp"

#include <vector>

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace demiplane::http {

    void ConnectionTracker::Handle::release() noexcept {
        if (tracker_ == nullptr) {
            return;
        }
        std::lock_guard lk{tracker_->mu_};
        tracker_->entries_.erase(it_);
        tracker_->in_flight_.fetch_sub(1, std::memory_order_acq_rel);
        tracker_ = nullptr;
    }

    boost::asio::awaitable<void> ConnectionTracker::drain_until(const boost::asio::any_io_executor ex,
                                                                const std::chrono::steady_clock::time_point deadline) {
        using namespace std::chrono_literals;
        boost::asio::steady_timer timer{ex};

        while (in_flight_.load(std::memory_order_acquire) > 0 && std::chrono::steady_clock::now() < deadline) {
            timer.expires_after(20ms);
            co_await timer.async_wait(boost::asio::use_awaitable);
        }

        // Snapshot the survivors under the lock; fire the cancel thunks after
        // releasing it (a thunk dispatches onto another strand, and a completing
        // connection's Handle dtor also takes the lock — do not hold it across).
        std::vector<Entry> survivors;
        {
            std::lock_guard lk{mu_};
            survivors.reserve(entries_.size());
            for (const auto& e : entries_) {
                survivors.push_back(e);
            }
        }
        for (const auto& e : survivors) {
            if (auto conn = e.conn.lock()) {  // skip connections that already finished
                e.cancel(conn);               // `conn` kept alive across the dispatch by capture
            }
        }
        // TODO(PR5): this only DISPATCHES force-cancels; it does not wait for the cancelled serve() coroutines to
        // unwind (in_flight_ -> 0). Safe to destroy the owning listener after drain ONLY on a single-threaded executor
        // (v1). For the multi-worker Server, poll until in_flight_==0 (short grace) or await a completion signal fired
        // by the last Handle::release().
        co_return;
    }

}  // namespace demiplane::http
