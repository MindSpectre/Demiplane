#pragma once

#include <algorithm>
#include <chrono>
#include <thread>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gears_concepts.hpp>

namespace demiplane::chrono {
    // Exponential backoff: base·2^attempt, clamped to cap. attempt is 0-based
    // (attempt 0 → base). The shift is clamped before the min so 1u << shift
    // can never overflow, regardless of how large a cap the caller passes.
    [[nodiscard]] constexpr std::chrono::milliseconds
    exponential_backoff(const std::uint32_t attempt,
                        const std::chrono::milliseconds base,
                        const std::chrono::milliseconds cap) noexcept {
        const std::uint32_t shift = std::min(attempt, 20u);
        return std::min(base * (1u << shift), cap);
    }

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
}  // namespace demiplane::chrono
