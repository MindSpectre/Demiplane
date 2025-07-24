#pragma once

#include <gears_templates.hpp>

template <typename... Args, typename Callable>
    requires std::invocable<Callable, Args...>
auto demiplane::chrono::Timer::execute_polite_vanish(const std::chrono::milliseconds timeout,
                                                     Callable&& fn,
                                                     Args&&... args) {
    // Extract the token reference from arguments
    static_assert(gears::has_exact_arg_type<CancellationToken&, Args...>(),
                  "No task properties found in arguments");

    // Extract the token reference using the generic get_arg function
    CancellationToken& ext_tok = gears::get_arg<CancellationToken&>(args...);


    using result_t = std::invoke_result_t<Callable, Args...>;

    // Use a lambda instead of std::bind to avoid copying non-copyable token
    std::packaged_task<result_t()> task(
        [fn = std::forward<Callable>(fn), args_tuple = std::forward_as_tuple(args...)]() mutable -> result_t {
            return std::apply(fn, std::move(args_tuple));
        });

    auto fut      = task.get_future();
    auto deadline = clock::now() + timeout;

    // worker
    auto worker = pool_->enqueue([t = std::move(task)]() mutable {
        t();
    });

    // watchdog - fixed
    spawn([&ext_tok, deadline]() mutable {
        while (!ext_tok.stop_requested() && clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
        ext_tok.cancel(); // polite request to worker
    });

    return fut;
}

template <typename... Args, typename Callable>
    requires std::invocable<Callable, Args...>
auto demiplane::chrono::Timer::execute_violent_kill(const std::chrono::milliseconds timeout,
                                                    CancellationToken& token,
                                                    Callable&& fn,
                                                    Args&&... args) {
    using result_t = std::invoke_result_t<Callable, Args...>;

    std::packaged_task<result_t()> task(std::bind(std::forward<Callable>(fn), std::forward<Args>(args)...));
    auto fut            = task.get_future();
    const auto deadline = clock::now() + timeout;

    // worker
    auto th = std::thread(std::move(task));

    // watchdog
    spawn([&deadline, &token, &th, h = th.native_handle()]() mutable {
        while (clock::now() < deadline && !token.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        #if defined(_WIN32)
            ::TerminateThread(h, 1); // dangerous!
        #elif defined(__linux__)
        ::pthread_cancel(h); // UB if locks held
        #endif

        if (th.joinable()) th.detach();
    });

    return fut;
}
