#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

#include <benchmark/benchmark.h>

namespace bench::pool {

    struct alignas(64) LatencyCollector {
        std::vector<std::chrono::nanoseconds> samples;

        void reserve(const std::size_t n) {
            samples.reserve(n);
        }

        void record(const std::chrono::nanoseconds ns) {
            samples.push_back(ns);
        }

        void clear() noexcept {
            samples.clear();
        }
    };

    inline void merge_latency(benchmark::State& state, const std::vector<LatencyCollector>& collectors) {
        std::vector<std::chrono::nanoseconds> merged;
        std::size_t total = 0;
        for (const auto& [samples] : collectors) {
            total += samples.size();
        }
        merged.reserve(total);

        for (const auto& [samples] : collectors) {
            merged.insert(merged.end(), samples.begin(), samples.end());
        }

        if (merged.empty()) {
            return;
        }

        std::ranges::sort(merged);

        auto percentile = [&](const double p) -> double {
            const auto idx = static_cast<std::size_t>(p * static_cast<double>(merged.size() - 1));
            return static_cast<double>(merged[idx].count()) / 1000.0;  // ns -> us
        };

        state.counters["p50_us"] = percentile(0.50);
        state.counters["p95_us"] = percentile(0.95);
        state.counters["p99_us"] = percentile(0.99);
    }

}  // namespace bench::pool
