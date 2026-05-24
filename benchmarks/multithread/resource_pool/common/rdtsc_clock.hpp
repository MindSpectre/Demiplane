#pragma once

#include <chrono>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    #include <immintrin.h>
    #include <x86intrin.h>  // __rdtscp lives here on clang/libc++
#else
    #error "rdtsc_clock.hpp currently supports x86 only — extend for ARM if needed"
#endif

namespace bench::pool {

    /// `rdtscp` carries an implicit serialization-of-prior-loads (lighter than
    /// LFENCE+RDTSC but enough for benchmark sampling — Intel SDM Vol 2:
    /// "RDTSCP waits until all previous instructions have been executed before
    /// reading the counter"). Cost is ~5-10 cycles on modern x86 vs ~30+
    /// cycles for the lfence+rdtsc pair, dropping the per-measurement floor
    /// by ~5 ns and making the per-acquire scenarios (Steady, TimeoutPressure,
    /// AsioPost*) report numbers closer to the true acquire latency.
    inline std::uint64_t rdtsc_now() noexcept {
        unsigned int aux;
        return __rdtscp(&aux);
    }

    struct RdtscCalib {
        double cycles_per_ns;

        static RdtscCalib measure() noexcept {
            using clock           = std::chrono::steady_clock;
            constexpr auto window = std::chrono::milliseconds(20);
            const auto t_start    = clock::now();
            const auto c_start    = rdtsc_now();
            while (clock::now() - t_start < window) {
                _mm_pause();
            }
            const auto t_end           = clock::now();
            const auto c_end           = rdtsc_now();
            const auto ns_elapsed      = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
            const double cycles_per_ns = static_cast<double>(c_end - c_start) / static_cast<double>(ns_elapsed);
            return RdtscCalib{cycles_per_ns};
        }
    };

    inline const RdtscCalib& rdtsc_calib() noexcept {
        static const RdtscCalib c = RdtscCalib::measure();
        return c;
    }

    inline std::chrono::nanoseconds cycles_to_ns(const std::uint64_t cycles) noexcept {
        const double ns = static_cast<double>(cycles) / rdtsc_calib().cycles_per_ns;
        return std::chrono::nanoseconds{static_cast<std::int64_t>(ns)};
    }

    inline std::uint64_t ns_to_cycles(const std::chrono::nanoseconds ns) noexcept {
        const double c = static_cast<double>(ns.count()) * rdtsc_calib().cycles_per_ns;
        return static_cast<std::uint64_t>(c);
    }

}  // namespace bench::pool
