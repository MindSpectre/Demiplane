#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

#include <pause.hpp>

#include "rdtsc_clock.hpp"

namespace bench::pool {

    /// Cheap holder for the pool. work_for() is an rdtsc-spin busy-wait that
    /// the compiler cannot elide (rdtsc is an intrinsic with side effects),
    /// so no volatile/atomic counter is needed for dce protection.
    struct MockResource {
        std::size_t id{};

        MockResource() noexcept = default;
        explicit MockResource(const std::size_t i) noexcept
            : id(i) {
        }

        // NOLINTNEXTLINE(readability-convert-member-functions-to-static) — keeps callable shape `r->work_for(...)`
        void work_for(const std::chrono::nanoseconds d) noexcept {
            if (d.count() <= 0) {
                return;
            }
            const std::uint64_t deadline = rdtsc_now() + ns_to_cycles(d);
            while (rdtsc_now() < deadline) {
                demiplane::multithread::pause_arc_agnostic();
            }
        }
    };

}  // namespace bench::pool
