#include "blocking_pool.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include <boost/asio/error.hpp>
#include <boost/asio/post.hpp>

namespace demiplane::db::postgres {

    BlockingPool::BlockingPool(ConnectionConfig connection_config, PoolConfig pool_config)
        : conn_cfg_{std::move(connection_config)},
          pool_cfg_{std::move(pool_config)} {
        const auto cap = pool_cfg_.capacity();
        holders_.reserve(cap);

        const auto min_conns = pool_cfg_.min_connections();
        for (std::size_t i = 0; i < min_conns; ++i) {
            if (auto holder = try_create_holder()) {
                ++created_;
                free_.push_back(holder.get());
                holders_.push_back(std::move(holder));
            }
        }
    }

    BlockingPool::~BlockingPool() {
        shutdown();
    }

    std::shared_ptr<QueuedHolder> BlockingPool::try_create_holder() {
        PGconn* c = create_connection();
        if (!c)
            return nullptr;
        return std::make_shared<QueuedHolder>(c, this);
    }

    PGconn* BlockingPool::create_connection() const {
        const auto s = conn_cfg_.to_connection_string();
        PGconn* c    = PQconnectdb(s.c_str());
        if (!c || PQstatus(c) != CONNECTION_OK) {
            if (c)
                PQfinish(c);
            return nullptr;
        }
        return c;
    }

    std::weak_ptr<ConnectionHolder> BlockingPool::try_acquire() noexcept {
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        std::unique_lock lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        if (!free_.empty()) {
            auto* raw = free_.front();
            free_.pop_front();
            return raw->weak_from_this();
        }

        if (created_ < pool_cfg_.capacity()) {
            lk.unlock();
            auto holder = try_create_holder();
            lk.lock();
            if (holder) {
                ++created_;
                auto w = holder->weak_from_this();
                holders_.push_back(std::move(holder));
                return w;
            }
        }
        return {};
    }

    std::weak_ptr<ConnectionHolder> BlockingPool::acquire(std::chrono::steady_clock::duration timeout) noexcept {
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        std::unique_lock lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        if (!free_.empty()) {
            auto* raw = free_.front();
            free_.pop_front();
            return raw->weak_from_this();
        }

        if (created_ < pool_cfg_.capacity()) {
            lk.unlock();
            auto holder = try_create_holder();
            lk.lock();
            if (holder) {
                ++created_;
                auto w = holder->weak_from_this();
                holders_.push_back(std::move(holder));
                return w;
            }
        }

        Waiter w;
        waiters_.emplace_back(&w);

        w.cv.wait_for(lk, timeout, [&] { return w.ready || shutdown_.load(std::memory_order_acquire); });

        if (w.ready) {
            return w.assigned->weak_from_this();
        }
        std::erase_if(waiters_, [&](const WaiterEntry& e) {
            const auto* p = std::get_if<Waiter*>(&e);
            return p != nullptr && *p == &w;
        });
        return {};
    }

    std::weak_ptr<ConnectionHolder> BlockingPool::acquire() noexcept {
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        std::unique_lock lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire))
            return {};

        if (!free_.empty()) {
            auto* raw = free_.front();
            free_.pop_front();
            return raw->weak_from_this();
        }

        if (created_ < pool_cfg_.capacity()) {
            lk.unlock();
            auto holder = try_create_holder();
            lk.lock();
            if (holder) {
                ++created_;
                auto ww = holder->weak_from_this();
                holders_.push_back(std::move(holder));
                return ww;
            }
        }

        Waiter w;
        waiters_.emplace_back(&w);

        w.cv.wait(lk, [&] { return w.ready || shutdown_.load(std::memory_order_acquire); });

        if (w.ready) {
            return w.assigned->weak_from_this();
        }
        std::erase_if(waiters_, [&](const WaiterEntry& e) {
            const auto* p = std::get_if<Waiter*>(&e);
            return p != nullptr && *p == &w;
        });
        return {};
    }

    void BlockingPool::initiate_async_acquire(const boost::asio::any_io_executor& exec,
                                              const std::chrono::steady_clock::duration timeout,
                                              AsyncWaiter::Handler handler) {
        const auto post_error =
            [](const boost::asio::any_io_executor& e, AsyncWaiter::Handler h, boost::system::error_code ec) {
                boost::asio::post(
                    e, [h = std::move(h), ec]() mutable { std::move(h)(ec, std::shared_ptr<QueuedHolder>{}); });
            };

        const auto post_success =
            [](const boost::asio::any_io_executor& e, AsyncWaiter::Handler h, std::shared_ptr<QueuedHolder> holder_sp) {
                boost::asio::post(e, [h = std::move(h), holder_sp = std::move(holder_sp)]() mutable {
                    std::move(h)(boost::system::error_code{}, std::move(holder_sp));
                });
            };

        if (shutdown_.load(std::memory_order_acquire)) {
            post_error(exec, std::move(handler), boost::asio::error::operation_aborted);
            return;
        }

        std::unique_lock lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire)) {
            lk.unlock();
            post_error(exec, std::move(handler), boost::asio::error::operation_aborted);
            return;
        }

        // Fast path: free holder available.
        if (!free_.empty()) {
            auto* raw = free_.front();
            free_.pop_front();
            auto holder_sp = raw->shared_from_this();
            lk.unlock();
            post_success(exec, std::move(handler), std::move(holder_sp));
            return;
        }

        // Lazy creation up to capacity.
        if (created_ < pool_cfg_.capacity()) {
            lk.unlock();
            auto holder = try_create_holder();
            lk.lock();
            if (shutdown_.load(std::memory_order_acquire)) {
                lk.unlock();
                post_error(exec, std::move(handler), boost::asio::error::operation_aborted);
                return;
            }
            if (holder) {
                ++created_;
                auto holder_sp = holder;
                holders_.push_back(std::move(holder));
                lk.unlock();
                post_success(exec, std::move(handler), std::move(holder_sp));
                return;
            }
        }

        // Slow path: queue as async waiter.
        auto waiter     = std::make_shared<AsyncWaiter>(exec);
        waiter->handler = std::move(handler);

        const bool bounded = timeout > std::chrono::steady_clock::duration::zero();
        if (bounded) {
            waiter->timer.expires_after(timeout);
        }

        waiters_.emplace_back(waiter);

        if (bounded) {
            std::weak_ptr<AsyncWaiter> weak = waiter;
            waiter->timer.async_wait([this, weak](const boost::system::error_code& ec) {
                if (ec == boost::asio::error::operation_aborted)
                    return;
                const auto w = weak.lock();
                if (!w)
                    return;
                on_async_waiter_timeout(w);
            });
        }
        // Unbounded path: timer is never armed. The waiter stays in waiters_
        // until a release (return_holder) or shutdown claims it.
    }

    void BlockingPool::on_async_waiter_timeout(const std::shared_ptr<AsyncWaiter>& waiter) noexcept {
        std::unique_lock lk{mtx_};

        if (bool expected = false;
            !waiter->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            // Already claimed by release or shutdown — nothing to do.
            return;
        }

        std::erase_if(waiters_, [&](const WaiterEntry& e) {
            const auto* sp = std::get_if<std::shared_ptr<AsyncWaiter>>(&e);
            return sp != nullptr && sp->get() == waiter.get();
        });

        auto handler    = std::move(waiter->handler);
        const auto exec = std::move(waiter->caller_exec);
        lk.unlock();

        boost::asio::post(exec, [h = std::move(handler)]() mutable {
            std::move(h)(boost::asio::error::timed_out, std::shared_ptr<QueuedHolder>{});
        });
    }

    void BlockingPool::return_holder(QueuedHolder* raw) noexcept {
        std::unique_lock lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire))
            return;

        while (!waiters_.empty()) {
            WaiterEntry entry = std::move(waiters_.front());
            waiters_.pop_front();

            if (auto** wpp = std::get_if<Waiter*>(&entry)) {
                Waiter* w   = *wpp;
                w->assigned = raw;
                w->ready    = true;
                lk.unlock();
                w->cv.notify_one();
                return;
            }

            // std::shared_ptr<AsyncWaiter>
            const auto& waiter_sp = std::get<std::shared_ptr<AsyncWaiter>>(entry);

            if (bool expected = false;
                !waiter_sp->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                // Timer already claimed this waiter — skip and try next.
                continue;
            }

            // Best-effort timer cancellation. The timer callback holds a
            // weak_ptr and will no-op on claim-loss anyway.
            waiter_sp->timer.cancel();

            auto handler    = std::move(waiter_sp->handler);
            const auto exec = std::move(waiter_sp->caller_exec);
            lk.unlock();

            auto holder_sp = raw->shared_from_this();
            boost::asio::post(exec, [h = std::move(handler), holder_sp = std::move(holder_sp)]() mutable {
                std::move(h)(boost::system::error_code{}, std::move(holder_sp));
            });
            return;
        }

        // No waiter consumed the holder; park it in the free list.
        free_.push_back(raw);
    }

    void BlockingPool::drop_dead(QueuedHolder* raw) noexcept {
        std::lock_guard lk{mtx_};
        if (shutdown_.load(std::memory_order_acquire))
            return;

        if (const auto it = std::ranges::find_if(holders_, [raw](const auto& h) { return h.get() == raw; });
            it != holders_.end()) {
            holders_.erase(it);
            --created_;
        }
    }

    void BlockingPool::shutdown() {
        if (shutdown_.exchange(true, std::memory_order_acq_rel))
            return;

        // Collect async handlers to dispatch outside the mutex.
        std::vector<std::pair<boost::asio::any_io_executor, AsyncWaiter::Handler>> async_handlers;

        {
            std::lock_guard lk{mtx_};
            for (auto& entry : waiters_) {
                if (auto** wpp = std::get_if<Waiter*>(&entry)) {
                    (*wpp)->cv.notify_one();
                    continue;
                }
                const auto& waiter_sp = std::get<std::shared_ptr<AsyncWaiter>>(entry);
                if (bool expected = false;
                    !waiter_sp->claimed.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                    // Timer already won; it will post its own timeout.
                    continue;
                }
                waiter_sp->timer.cancel();
                async_handlers.emplace_back(std::move(waiter_sp->caller_exec), std::move(waiter_sp->handler));
            }
            waiters_.clear();
            free_.clear();
            holders_.clear();
        }

        for (auto& [exec, handler] : async_handlers) {
            boost::asio::post(exec, [h = std::move(handler)]() mutable {
                std::move(h)(boost::asio::error::operation_aborted, std::shared_ptr<QueuedHolder>{});
            });
        }
    }

    std::size_t BlockingPool::capacity() const noexcept {
        return pool_cfg_.capacity();
    }

    std::size_t BlockingPool::free_count() const noexcept {
        std::lock_guard lk{mtx_};
        return free_.size();
    }

    std::size_t BlockingPool::active_count() const noexcept {
        std::lock_guard lk{mtx_};
        return created_ - free_.size();
    }

    std::size_t BlockingPool::waiter_count() const noexcept {
        std::lock_guard lk{mtx_};
        return waiters_.size();
    }

    bool BlockingPool::is_shutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }

    const ConnectionConfig& BlockingPool::connection_config() const noexcept {
        return conn_cfg_;
    }
    const PoolConfig& BlockingPool::pool_config() const noexcept {
        return pool_cfg_;
    }

}  // namespace demiplane::db::postgres
