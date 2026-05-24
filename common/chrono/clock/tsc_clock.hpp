#pragma once

#include <chrono>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
    #include <x86intrin.h>  // __rdtscp lives here on both clang/libc++ and gcc/libstdc++
    #define DMP_TSC_X86 1
#elif defined(__aarch64__)
    #define DMP_TSC_ARM64 1
#else
    #error "TscClock supports x86 and AArch64 only — add the arch's cycle-counter read to now()"
#endif

namespace demiplane::chrono {

    /// Monotonic hardware tick counter for fine-grained latency sampling, where
    /// `steady_clock`'s ~20-40 ns call overhead would dominate the measurement.
    ///
    /// `now()` reads a raw hardware counter:
    ///   - x86:     `rdtscp`, which carries an implicit serialization-of-prior-loads
    ///              (lighter than `lfence`+`rdtsc` but enough for sampling — Intel SDM
    ///              Vol 2: "RDTSCP waits until all previous instructions have been
    ///              executed before reading the counter").
    ///   - AArch64: the `cntvct_el0` virtual count register behind an `isb`, giving the
    ///              same "prior instructions retired" intent as `rdtscp`.
    ///
    /// The counter is in architecture-defined ticks, NOT nanoseconds: convert deltas with
    /// `to_duration()` / `to_cycles()`. Those use a one-shot calibration against
    /// `steady_clock` (measured on first use, then cached). On x86 the TSC is invariant on
    /// every CPU since ~Nehalem; on AArch64 `cntvct_el0` is a fixed-frequency system
    /// counter — both are stable enough that a single calibration holds for the process
    /// lifetime. Tick deltas are meaningful only within one process on one machine.
    class TscClock {
    public:
        /// Raw counter read. Units are architecture-defined ticks (see class docs).
        [[nodiscard]] static std::uint64_t now() noexcept {
#if defined(DMP_TSC_X86)
            unsigned int aux;
            return __rdtscp(&aux);
#elif defined(DMP_TSC_ARM64)
            std::uint64_t ticks;
            asm volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(ticks)::"memory");
            return ticks;
#endif
        }

        /// Calibrated counter ticks per nanosecond. Measured once against `steady_clock`
        /// over a short window, then cached for the process lifetime.
        [[nodiscard]] static double cycles_per_ns() noexcept {
            static const double cpn = measure_cycles_per_ns();
            return cpn;
        }

        /// Convert a counter delta (e.g. `now() - earlier`) to a duration.
        [[nodiscard]] static std::chrono::nanoseconds to_duration(const std::uint64_t ticks) noexcept {
            return std::chrono::nanoseconds{static_cast<std::int64_t>(static_cast<double>(ticks) / cycles_per_ns())};
        }

        /// Convert a duration to a counter delta (e.g. for a spin deadline `now() + to_cycles(d)`).
        [[nodiscard]] static std::uint64_t to_cycles(const std::chrono::nanoseconds duration) noexcept {
            return static_cast<std::uint64_t>(static_cast<double>(duration.count()) * cycles_per_ns());
        }

        /// Nanoseconds elapsed since an earlier `now()` reading.
        [[nodiscard]] static std::chrono::nanoseconds elapsed(const std::uint64_t since) noexcept {
            return to_duration(now() - since);
        }

    private:
        [[nodiscard]] static double measure_cycles_per_ns() noexcept {
            using steady           = std::chrono::steady_clock;
            constexpr auto window  = std::chrono::milliseconds{20};
            // Read steady-then-tsc at both ends so the tsc interval is shifted by one
            // clock read at each end and stays the same length as the steady interval.
            const auto t0          = steady::now();
            const std::uint64_t c0 = now();
            while (steady::now() - t0 < window) {
                // Busy spin. steady_clock::now() has observable side effects, so the loop
                // is not elided; this runs once at startup, so the spin cost is irrelevant.
            }
            const auto t1          = steady::now();
            const std::uint64_t c1 = now();
            const auto ns          = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            return static_cast<double>(c1 - c0) / static_cast<double>(ns);
        }
    };

}  // namespace demiplane::chrono
