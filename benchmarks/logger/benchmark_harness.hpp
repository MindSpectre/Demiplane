#pragma once

#include <barrier>
#include <chrono>
#include <demiplane/ink>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace bench {

    constexpr std::size_t CONTENTION_THREADS    = 8;
    constexpr std::size_t BASELINE_THREADS      = 1;
    constexpr std::size_t ITERATIONS_PER_THREAD = 1'000'000;
    constexpr std::size_t TARGET_RECORD_SIZE    = 256;

    struct ThreadResult {
        std::chrono::nanoseconds completion_time{};
    };

    struct BenchmarkResult {
        std::size_t thread_count                 = 0;
        std::size_t iterations                   = 0;
        std::vector<ThreadResult> thread_results = {};
        std::chrono::nanoseconds wall_clock      = {};
        double entries_per_second                = 0.0;
    };

    inline void
    print_results(const BenchmarkResult& result, const std::string_view prefix, const std::string_view title) {
        const double wall_sec = static_cast<double>(result.wall_clock.count()) / 1e9;

        std::chrono::nanoseconds min_time = result.thread_results[0].completion_time;
        std::chrono::nanoseconds max_time = result.thread_results[0].completion_time;
        std::chrono::nanoseconds sum_time{};

        for (const auto& tr : result.thread_results) {
            min_time  = std::min(min_time, tr.completion_time);
            max_time  = std::max(max_time, tr.completion_time);
            sum_time += tr.completion_time;
        }

        const double avg_sec =
            static_cast<double>(sum_time.count()) / static_cast<double>(result.thread_results.size()) / 1e9;
        const double min_sec = static_cast<double>(min_time.count()) / 1e9;
        const double max_sec = static_cast<double>(max_time.count()) / 1e9;

        const std::string body = demiplane::ink::section("")
                                     .row("Threads", result.thread_count)
                                     .row("Iterations/thread", result.iterations)
                                     .row("Target record", "~256 bytes")
                                     .row("Wall-clock", std::format("{:.3f} s", wall_sec))
                                     .row("Throughput", std::format("{:.0f} ops/s", result.entries_per_second))
                                     .row("Avg thread time", std::format("{:.3f} s", avg_sec))
                                     .row("Min thread time", std::format("{:.3f} s", min_sec))
                                     .row("Max thread time", std::format("{:.3f} s", max_sec))
                                     .indent_size(1)
                                     .value_align(demiplane::ink::Align::Right)
                                     .render();

        std::cout << '\n'
                  << demiplane::ink::box(body)
                         .title(std::format("{}: {}", prefix, title))
                         .border(demiplane::ink::border::unicode)
                         .border_style(demiplane::ink::colors::bold_cyan)
                         .padding(1)
                         .terminate()
                         .render();
    }

    /**
     * @brief Run a multi-threaded benchmark with barrier sync and per-thread timing
     *
     * @tparam LogFn Callable invoked per iteration: log_fn()
     * @param thread_count Number of producer threads
     * @param iterations Number of iterations per thread
     * @param log_fn The logging call to benchmark
     * @return BenchmarkResult with timing and throughput
     */
    template <typename LogFn>
    BenchmarkResult
    run_threaded_benchmark(const std::size_t thread_count, const std::size_t iterations, LogFn&& log_fn) {
        std::barrier sync_point{static_cast<std::ptrdiff_t>(thread_count)};
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < iterations; ++j) {
                    log_fn();
                }

                const auto thread_end              = std::chrono::steady_clock::now();
                thread_results[id].completion_time = thread_end - thread_start;
                sync_point.arrive_and_wait();
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        const auto wall_end = std::chrono::steady_clock::now();

        const auto wall_clock    = wall_end - wall_start;
        const double wall_sec    = std::chrono::duration<double>(wall_clock).count();
        const auto total_entries = static_cast<double>(thread_count * iterations);

        return BenchmarkResult{
            .thread_count       = thread_count,
            .iterations         = iterations,
            .thread_results     = std::move(thread_results),
            .wall_clock         = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock),
            .entries_per_second = total_entries / wall_sec,
        };
    }

    /**
     * @brief Same as run_threaded_benchmark but wall-clock includes a post-join callback (e.g. shutdown)
     *
     * Measures true end-to-end time: producer work + async drain + shutdown.
     */
    template <typename LogFn, typename ShutdownFn>
    BenchmarkResult run_threaded_benchmark_e2e(const std::size_t thread_count,
                                               const std::size_t iterations,
                                               LogFn&& log_fn,
                                               ShutdownFn&& shutdown_fn) {
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < iterations; ++j) {
                    log_fn();
                }

                thread_results[id].completion_time = std::chrono::steady_clock::now() - thread_start;
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        shutdown_fn();

        const auto wall_end = std::chrono::steady_clock::now();

        const auto wall_clock    = wall_end - wall_start;
        const double wall_sec    = std::chrono::duration<double>(wall_clock).count();
        const auto total_entries = static_cast<double>(thread_count * iterations);

        return BenchmarkResult{
            .thread_count       = thread_count,
            .iterations         = iterations,
            .thread_results     = std::move(thread_results),
            .wall_clock         = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock),
            .entries_per_second = total_entries / wall_sec,
        };
    }

}  // namespace bench
