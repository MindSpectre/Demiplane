#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

#include <benchmark/benchmark.h>

namespace bench::pg {

    struct alignas(64) LatencyCollector {
        std::vector<std::chrono::nanoseconds> samples;

        void reserve(std::size_t n) {
            samples.reserve(n);
        }

        void record(std::chrono::nanoseconds ns) {
            samples.push_back(ns);
        }

        void clear() noexcept {
            samples.clear();
        }

        void report_to(benchmark::State& state) const {
            if (samples.empty()) {
                return;
            }

            auto sorted = samples;
            std::ranges::sort(sorted);

            auto percentile = [&](double p) -> double {
                const auto idx = static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1));
                return static_cast<double>(sorted[idx].count()) / 1000.0;  // ns -> us
            };

            state.counters["p50_us"] = percentile(0.50);
            state.counters["p95_us"] = percentile(0.95);
            state.counters["p99_us"] = percentile(0.99);
        }
    };

    inline void merge_latency(benchmark::State& state, const std::vector<LatencyCollector>& collectors) {
        std::vector<std::chrono::nanoseconds> merged;
        std::size_t total = 0;
        for (const auto& c : collectors) {
            total += c.samples.size();
        }
        merged.reserve(total);

        for (const auto& c : collectors) {
            merged.insert(merged.end(), c.samples.begin(), c.samples.end());
        }

        if (merged.empty()) {
            return;
        }

        std::ranges::sort(merged);

        auto percentile = [&](double p) -> double {
            const auto idx = static_cast<std::size_t>(p * static_cast<double>(merged.size() - 1));
            return static_cast<double>(merged[idx].count()) / 1000.0;  // ns -> us
        };

        state.counters["p50_us"] = percentile(0.50);
        state.counters["p95_us"] = percentile(0.95);
        state.counters["p99_us"] = percentile(0.99);
    }

    template <typename Fn>
    void timed_op(LatencyCollector& collector, Fn&& fn) {
        const auto start = std::chrono::steady_clock::now();
        fn();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        collector.record(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed));
    }

}  // namespace bench::pg
