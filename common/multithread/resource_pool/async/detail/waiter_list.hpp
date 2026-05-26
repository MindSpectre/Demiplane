#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/post.hpp>

namespace demiplane::multithread::detail {
    /// Why a parked waiter was resumed.
    enum class WaitOutcome : std::uint8_t { woken, cancelled, shut_down };

    /// Terminal state of a waiter node; `parked` is the only non-terminal value.
    /// Exactly one CAS off `parked` wins and drives the single completion.
    enum class WaitState : std::uint8_t { parked, notified, cancelled, shut_down };

    [[nodiscard]] inline WaitOutcome to_outcome(const WaitState s) noexcept {
        switch (s) {
            case WaitState::notified:
                return WaitOutcome::woken;
            case WaitState::cancelled:
                return WaitOutcome::cancelled;
            case WaitState::shut_down:
                return WaitOutcome::shut_down;
            case WaitState::parked:
                return WaitOutcome::woken;  // unreachable
        }
        return WaitOutcome::woken;
    }

    /// One waiting coroutine. Lives in the coroutine frame — never heap-allocated by
    /// the pool. Linked into WaiterList while parked.
    struct WaiterNode {
        WaiterNode* prev = nullptr;
        WaiterNode* next = nullptr;
        std::atomic<WaitState> state{WaitState::parked};
        boost::asio::any_io_executor exec{};
        boost::asio::any_completion_handler<void(WaitOutcome)> handler{};
    };

    /// Mutex-guarded doubly-linked FIFO list of parked waiters.
    ///
    /// The mutex is taken ONLY on link / wake / complete / drain — never on the
    /// pool's lock-free bitset fast path. wake_one wakes at most one waiter (one
    /// freed bit => one slot); a single per-node `state` CAS arbitrates the race
    /// between a release waking the node, a timeout/cancel completing it, and
    /// shutdown draining it. The winner unlinks under the mutex BEFORE the resume
    /// is posted, so the frame node is detached before its coroutine can run and
    /// destroy the frame.
    class WaiterList {
    public:
        void link(WaiterNode& n) noexcept {
            const std::lock_guard lk{mtx_};
            n.prev                                   = tail_;
            n.next                                   = nullptr;
            (tail_ != nullptr ? tail_->next : head_) = &n;
            tail_                                    = &n;
        }

        /// Drive `n` to terminal state `why` and post its resume. Returns true iff
        /// THIS call won the CAS (and thus owns the completion). Thread-safe.
        bool complete(WaiterNode& n, const WaitState why) noexcept {
            if (auto expected = WaitState::parked;
                !n.state.compare_exchange_strong(expected, why, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return false;
            }
            boost::asio::any_completion_handler<void(WaitOutcome)> h;
            boost::asio::any_io_executor ex;
            {
                const std::lock_guard lk{mtx_};
                unlink_locked(n);
                h  = std::move(n.handler);
                ex = std::move(n.exec);
            }
            post_resume(ex, std::move(h), to_outcome(why));
            return true;
        }

        /// Wake at most one still-parked waiter, FIFO. Called on slot release.
        void wake_one() noexcept {
            bool found = false;
            boost::asio::any_completion_handler<void(WaitOutcome)> h;
            boost::asio::any_io_executor ex;
            {
                const std::lock_guard lk{mtx_};
                for (WaiterNode* n = head_; n != nullptr; n = n->next) {
                    if (auto expected = WaitState::parked; n->state.compare_exchange_strong(
                            expected, WaitState::notified, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        unlink_locked(*n);
                        h     = std::move(n->handler);
                        ex    = std::move(n->exec);
                        found = true;
                        break;
                    }
                    // CAS failed: a timer/cancel/shutdown arm already claimed this
                    // node (unlink pending under its own lock). Skip to the next.
                }
            }
            if (found) {
                post_resume(ex, std::move(h), WaitOutcome::woken);
            }
        }

        /// Complete every still-parked waiter with `why` (shutdown drain).
        void drain_all(const WaitState why) noexcept {
            std::vector<std::pair<boost::asio::any_io_executor, boost::asio::any_completion_handler<void(WaitOutcome)>>>
                pending;
            {
                const std::lock_guard lk{mtx_};
                for (WaiterNode* n = head_; n != nullptr;) {
                    WaiterNode* const nxt = n->next;
                    if (auto expected = WaitState::parked; n->state.compare_exchange_strong(
                            expected, why, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        unlink_locked(*n);
                        pending.emplace_back(std::move(n->exec), std::move(n->handler));
                    }
                    n = nxt;
                }
            }
            const WaitOutcome outcome = to_outcome(why);
            for (auto& [ex, h] : pending) {
                post_resume(ex, std::move(h), outcome);
            }
        }

        [[nodiscard]] bool empty() const noexcept {
            const std::lock_guard lk{mtx_};
            return head_ == nullptr;
        }

    private:
        static void post_resume(const boost::asio::any_io_executor& ex,
                                boost::asio::any_completion_handler<void(WaitOutcome)> h,
                                const WaitOutcome outcome) noexcept {
            boost::asio::post(ex, [h = std::move(h), outcome]() mutable { std::move(h)(outcome); });
        }

        void unlink_locked(WaiterNode& n) noexcept {
            if (n.prev != nullptr) {
                n.prev->next = n.next;
            } else {
                head_ = n.next;
            }
            if (n.next != nullptr) {
                n.next->prev = n.prev;
            } else {
                tail_ = n.prev;
            }
            n.prev = nullptr;
            n.next = nullptr;
        }

        mutable std::mutex mtx_;
        WaiterNode* head_ = nullptr;
        WaiterNode* tail_ = nullptr;
    };

}  // namespace demiplane::multithread::detail
