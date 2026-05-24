#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "bench_scenarios.hpp"
#include "rdtsc_clock.hpp"

namespace bench::pool {

    /// Strip `--pin=0|1` out of argv (Google Benchmark doesn't know about it),
    /// return the parsed value. argc is mutated in place.
    inline bool parse_pin_flag(int& argc, char** argv) noexcept {
        bool pin = false;
        int dst  = 1;
        for (int i = 1; i < argc; ++i) {
            const std::string_view a{argv[i]};
            if (a == "--pin=0") {
                pin = false;
            } else if (a == "--pin=1") {
                pin = true;
            } else {
                argv[dst++] = argv[i];
            }
        }
        argc = dst;
        return pin;
    }

    inline std::string make_bench_name(const std::string_view subject, const std::string_view scenario_name) {
        std::string out;
        out.reserve(3 + subject.size() + 1 + scenario_name.size() + 1);
        out.append("BM_");
        out.append(subject);
        out.push_back('_');
        out.append(scenario_name);
        return out;
    }

    /// Print the rdtsc calibration once at startup so the user can sanity-check
    /// against `lscpu`'s reported max frequency.
    inline void print_calibration() {
        const double c = rdtsc_calib().cycles_per_ns;
        std::cerr << "[rdtsc_calib] cycles_per_ns = " << c << "  (=" << (c * 1000.0) << " MHz nominal)\n";
    }

    /// Cheap-to-read file helper. Returns empty string on read failure.
    inline std::string read_file_trim(const std::string& path) {
        std::ifstream f{path};
        if (!f.is_open())
            return {};
        std::string s;
        std::getline(f, s);
        while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
            s.pop_back();
        }
        return s;
    }

    /// Warn (don't fail) about environment knobs that add jitter to per-acquire
    /// latency measurements. Each warning is one line on stderr, prefixed
    /// `[env]`. Aim: the user can quickly see which knob to flip before
    /// trusting the p99 / p95 numbers.
    inline void env_check() {
        std::cerr << "[env] checking jitter sources (warnings only; bench will still run)\n";

        // CPU governor — only checks core 0 (the rest are usually the same)
        const std::string gov = read_file_trim("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
        if (!gov.empty() && gov != "performance") {
            std::cerr << "[env]   WARN  cpu0 governor = '" << gov
                      << "' (recommend 'performance' — `sudo cpupower frequency-set -g performance`)\n";
        }

        // SMT / hyper-threading. lscpu's "Thread(s) per core" is per
        // /sys/devices/system/cpu/smt/active (when present).
        const std::string smt = read_file_trim("/sys/devices/system/cpu/smt/active");
        if (smt == "1") {
            std::cerr << "[env]   WARN  SMT active — sibling threads share L1/L2 and add jitter "
                         "(`echo off | sudo tee /sys/devices/system/cpu/smt/control`)\n";
        }

        // C-state count on core 0 (any state > 1 means deeper sleeps possible)
        int c_states = 0;
        for (int s = 0; s < 16; ++s) {
            const std::string p = "/sys/devices/system/cpu/cpu0/cpuidle/state" + std::to_string(s) + "/disable";
            const std::string v = read_file_trim(p);
            if (v.empty())
                break;  // no more states
            if (v == "0")
                ++c_states;  // state is enabled
        }
        if (c_states > 1) {
            std::cerr << "[env]   WARN  " << c_states
                      << " C-states enabled on cpu0 — exits from C1+ cost 1-10 µs of jitter "
                         "(`sudo cpupower idle-set -D 0`)\n";
        }

        // Transparent hugepages
        const std::string thp = read_file_trim("/sys/kernel/mm/transparent_hugepage/enabled");
        if (thp.find("[never]") != std::string::npos) {
            std::cerr << "[env]   WARN  THP = 'never' — bitset and slot storage pay TLB cost "
                         "(`echo madvise | sudo tee /sys/kernel/mm/transparent_hugepage/enabled`)\n";
        }

        // Loadavg
        const std::string la = read_file_trim("/proc/loadavg");
        if (!la.empty()) {
            const double one_min = std::strtod(la.c_str(), nullptr);
            if (one_min > 2.0) {
                std::cerr << "[env]   WARN  loadavg (1m) = " << one_min
                          << " — other processes will preempt the bench cores\n";
            }
        }
    }

}  // namespace bench::pool
