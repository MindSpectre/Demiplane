// timer.hpp  — C++23, single header, snake_case identifiers
#pragma once
#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>
#include <demiplane/gears>
//-------------------------------------------------------------------
//public API
//-------------------------------------------------------------------
namespace demiplane::chrono {
    class cancellation_token : gears::NonCopyable {
    public:
        void cancel() noexcept {
            flag_.store(true, std::memory_order_consume);
        }

        [[nodiscard]] bool stop_requested() const noexcept {
            return flag_.load(std::memory_order_relaxed);
        }

    private:
        std::atomic_bool flag_{false};
    };

    class Timer {
    public:
        using clock = std::chrono::steady_clock;

        explicit Timer(std::chrono::nanoseconds timeout)
            : timeout_(timeout) {}

        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute(Callable&& fn, Args&&... args);

        template <typename... Args, typename Callable>
            requires std::invocable<Callable, cancellation_token&, Args...>
        auto execute_polite_vanish(cancellation_token& tok,
                                   Callable&& fn,
                                   Args&&... args);

        template <typename... Args, typename Callable>
            requires std::invocable<Callable, Args...>
        auto execute_violent_kill(cancellation_token& token, Callable&& fn, Args&&... args);

    private:
        std::chrono::nanoseconds timeout_;

        // helper - default spawns a jthread; replace with thread-pool later
        template <typename F>
        auto spawn(F&& f) {
            return std::jthread{std::forward<F>(f)};
        }

        // type-trait: does first arg look like a cancellation token?
        template <typename First, typename... Rest>
        static constexpr bool first_arg_is_token =
            std::is_same_v<std::remove_cvref_t<First>, cancellation_token> ||
            std::is_same_v<std::remove_cvref_t<First>, std::stop_token>;
    };

    //-------------------------------------------------------------------
    //implementation
    //-------------------------------------------------------------------
    template <typename... Args, typename Callable>
        requires std::invocable<Callable, Args...>
    auto Timer::execute(Callable&& fn, Args&&... args) {
        if constexpr (sizeof...(Args) > 0 &&
                      first_arg_is_token<std::tuple_element_t<0,
                                                              std::tuple<Args...>>, Args...>) {
            return execute_polite_vanish(std::get<0>(std::forward_as_tuple(args...)),
                                         std::forward<Callable>(fn),
                                         std::forward<Args>(args)...);
        }
        else {
            return execute_violent_kill(std::forward<Callable>(fn),
                                        std::forward<Args>(args)...);
        }
    }

    template <typename... Args, typename Callable>
        requires std::invocable<Callable, cancellation_token&, Args...>
    auto Timer::execute_polite_vanish(cancellation_token& ext_tok,
                                      Callable&& fn,
                                      Args&&... args) {
        using result_t = std::invoke_result_t<Callable, cancellation_token&, Args...>;

        std::packaged_task<result_t()> task(
            std::bind(std::forward<Callable>(fn),
                      std::ref(ext_tok),
                      std::forward<Args>(args)...));

        auto fut      = task.get_future();
        auto deadline = clock::now() + timeout_;

        // worker
        auto worker = spawn([t = std::move(task),
                tok = std::stop_source{}, &ext_tok]() mutable {
                std::stop_callback cb(tok.get_token(), [&ext_tok] {
                    ext_tok.cancel();
                });
                t();
            });

        // watchdog - fixed
        spawn([&ext_tok, deadline, st = worker.get_stop_source()]() mutable {
            while (!ext_tok.stop_requested() && clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            }
            st.request_stop(); // polite request to worker
        });

        return fut;
    }

    template <typename... Args, typename Callable>
        requires std::invocable<Callable, Args...>
    auto Timer::execute_violent_kill(cancellation_token& token, Callable&& fn, Args&&... args) {
        using result_t = std::invoke_result_t<Callable, Args...>;

        std::packaged_task<result_t()> task(
            std::bind(std::forward<Callable>(fn),
                      std::forward<Args>(args)...));
        auto fut            = task.get_future();
        const auto deadline = clock::now() + timeout_;

        // worker
        auto th = std::thread(std::move(task));

        // watchdog
        spawn([&deadline, &token, &th, h = th.native_handle()]() mutable {
            std::cout << "watchdog: " << std::this_thread::get_id() << std::endl;
            while (clock::now() < deadline && !token.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
            }

            #   if defined(_WIN32)
            ::TerminateThread(h, 1);     // dangerous!
            #   elif defined(__linux__)
            ::pthread_cancel(h); // UB if locks held
            #   endif

            if (th.joinable()) th.detach();
        });

        return fut;
    }
} // namespace util
