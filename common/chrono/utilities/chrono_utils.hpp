#pragma once

#include <thread>

#include <gears_concepts.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace demiplane::chrono {
    template <gears::IsDuration DurationClass>
    void sleep_for(const DurationClass duration) {
        std::this_thread::sleep_for(duration);
    }

    template <gears::IsDuration DurationClass>
    boost::asio::awaitable<void> async_sleep_for(const DurationClass duration) {
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer timer(executor);
        timer.expires_after(duration);
        co_await timer.async_wait(boost::asio::use_awaitable);
    }

    template <gears::IsDuration DurationClass>
    boost::asio::awaitable<void> async_sleep_for(const boost::asio::any_io_executor& executor,
                                                 const DurationClass duration) {
        boost::asio::steady_timer timer(executor);
        timer.expires_after(duration);
        co_await timer.async_wait(boost::asio::use_awaitable);
    }
} // namespace demiplane::chrono
