#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <latch>
#include <random>
#include <vector>

#include <benchmark/benchmark.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <gears_macros.hpp>

#include "bench_constants.hpp"
#include "bench_latency.hpp"
#include "bench_scenarios.hpp"

namespace bench::pg {

    // Duck-typed Backend concepts. Not enforced with C++20 concepts.
    //
    // SyncBackend requires:
    //     void post_task(std::function<void()> f);
    //     bool run_query(int id);
    //
    // AsyncBackend requires:
    //     boost::asio::io_context& io();
    //     boost::asio::awaitable<bool> run_query(
    //         int id, boost::asio::any_io_executor e);

    namespace detail {
        [[nodiscard]] inline std::mt19937& thread_rng() {
            thread_local std::mt19937 rng{std::random_device{}()};
            return rng;
        }

        inline void report_counters(::benchmark::State& state,
                                    const Scenario& scenario,
                                    std::int64_t completed,
                                    std::int64_t skipped,
                                    const std::vector<LatencyCollector>& collectors) {
            state.SetItemsProcessed(completed);
            state.counters["skipped"]     = static_cast<double>(skipped);
            state.counters["rps_ceiling"] = rps_ceiling(scenario);
            merge_latency(state, collectors);
        }
    }  // namespace detail

    template <typename SyncBackend>
    void run_sync_workload(::benchmark::State& state, const Scenario& scenario, SyncBackend& backend) {
        const auto workers          = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = scenario.queries_per_fireshot;

        std::vector<LatencyCollector> collectors(workers);
        std::atomic<std::int64_t> completed_ops{0};
        std::atomic<std::int64_t> skipped_ops{0};

        for (GEARS_UNUSED_VAR : state) {
            for (auto& c : collectors) {
                c.clear();
            }
            std::latch done{static_cast<std::ptrdiff_t>(workers)};

            for (std::size_t i = 0; i < workers; ++i) {
                backend.post_task([&, i] {
                    auto& rng = detail::thread_rng();
                    std::uniform_int_distribution<int> dist{1, MAX_ID};
                    for (int q = 0; q < queries_per_task; ++q) {
                        const int id       = dist(rng);
                        const auto start   = std::chrono::steady_clock::now();
                        const bool ok      = backend.run_query(id);
                        const auto elapsed = std::chrono::steady_clock::now() - start;
                        if (ok) {
                            collectors[i].record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
                            completed_ops.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            skipped_ops.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    done.count_down();
                });
            }

            done.wait();
        }

        detail::report_counters(state,
                                scenario,
                                completed_ops.load(std::memory_order_relaxed),
                                skipped_ops.load(std::memory_order_relaxed),
                                collectors);
    }

    template <typename AsyncBackend>
    void run_async_workload(::benchmark::State& state, const Scenario& scenario, AsyncBackend& backend) {
        const auto workers          = static_cast<std::size_t>(state.range(0));
        const auto queries_per_task = scenario.queries_per_fireshot;

        std::vector<LatencyCollector> collectors(workers);
        std::atomic<std::int64_t> completed_ops{0};
        std::atomic<std::int64_t> skipped_ops{0};

        for (GEARS_UNUSED_VAR : state) {
            for (auto& c : collectors) {
                c.clear();
            }
            std::latch done{static_cast<std::ptrdiff_t>(workers)};

            for (std::size_t i = 0; i < workers; ++i) {
                boost::asio::co_spawn(
                    backend.io(),
                    [&, i]() -> boost::asio::awaitable<void> {
                        auto& rng = detail::thread_rng();
                        std::uniform_int_distribution<int> dist{1, MAX_ID};
                        auto coro_exec = co_await boost::asio::this_coro::executor;
                        for (int q = 0; q < queries_per_task; ++q) {
                            const int id       = dist(rng);
                            const auto start   = std::chrono::steady_clock::now();
                            const bool ok      = co_await backend.run_query(id, coro_exec);
                            const auto elapsed = std::chrono::steady_clock::now() - start;
                            if (ok) {
                                collectors[i].record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
                                completed_ops.fetch_add(1, std::memory_order_relaxed);
                            } else {
                                skipped_ops.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                        done.count_down();
                    },
                    boost::asio::detached);
            }

            done.wait();
        }

        detail::report_counters(state,
                                scenario,
                                completed_ops.load(std::memory_order_relaxed),
                                skipped_ops.load(std::memory_order_relaxed),
                                collectors);
    }

}  // namespace bench::pg
