#pragma once

#include <atomic>
#include <memory>

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

namespace demiplane::db::postgres {

    class QueuedHolder;

    /**
     * Heap-allocated waiter node for BlockingPool::async_acquire.
     *
     * Unlike the sync Waiter (stack + CV), AsyncWaiter holds a type-erased
     * asio completion handler bound to the caller's executor and a
     * steady_timer for bounded waits. The atomic claimed flag resolves
     * the race between "pool released a slot to me" and "my timer fired";
     * whichever side flips the flag first drives the completion.
     *
     * Lifetime: owned by BlockingPool via std::shared_ptr while queued.
     * Timer callbacks hold only a std::weak_ptr, so shutdown clears the
     * deque and any lagging callback no-ops safely.
     */
    struct AsyncWaiter : gears::NonCopyable {
        using Handler =
            boost::asio::any_completion_handler<void(boost::system::error_code, std::shared_ptr<QueuedHolder>)>;

        explicit AsyncWaiter(boost::asio::any_io_executor exec) noexcept
            : caller_exec{std::move(exec)},
              timer{caller_exec} {
        }

        boost::asio::any_io_executor caller_exec;
        boost::asio::steady_timer timer;
        Handler handler{};
        std::atomic_bool claimed{false};
    };

}  // namespace demiplane::db::postgres
