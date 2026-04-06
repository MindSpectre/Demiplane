#include <barrier>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

namespace {

    constexpr std::size_t CONTENTION_THREADS    = 8;
    constexpr std::size_t BASELINE_THREADS      = 1;
    constexpr std::size_t ITERATIONS_PER_THREAD = 1'000'000;

    // Estimated Abseil prefix: ~70 bytes
    // Format: "I0405 17:17:23.123456 140424308053952 abseil_file_benchmark.cpp:XX] "
    constexpr std::size_t TARGET_RECORD_SIZE = 256;
    constexpr std::size_t ABSEIL_PREFIX_EST  = 70;
    constexpr std::size_t ABSEIL_PAD_SIZE    = TARGET_RECORD_SIZE - ABSEIL_PREFIX_EST;

    // Null sink: accepts events, does nothing. Measures pure Abseil overhead.
    class AbseilNullSink final : public absl::LogSink {
    public:
        void Send(const absl::LogEntry& /*entry*/) override {
        }
        void Flush() override {
        }
    };

    class AbseilFileSink final : public absl::LogSink {
    public:
        explicit AbseilFileSink(const std::string& path) {
            file_.rdbuf()->pubsetbuf(buffer_, sizeof(buffer_));
            file_.open(path, std::ios::out | std::ios::app);
            if (!file_.is_open()) {
                throw std::runtime_error{"Failed to open log file: " + path};
            }
        }

        ~AbseilFileSink() override {
            std::lock_guard lock{mutex_};
            if (file_.is_open()) {
                file_.flush();
                file_.close();
            }
        }

        void Send(const absl::LogEntry& entry) override {
            std::lock_guard lock{mutex_};
            file_ << entry.text_message_with_prefix() << '\n';
        }

        void Flush() override {
            std::lock_guard lock{mutex_};
            file_.flush();
        }

    private:
        std::ofstream file_;
        std::mutex mutex_;
        alignas(64) char buffer_[64 * 1024]{};
    };

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

    void print_results(const BenchmarkResult& result, const std::string& title) {
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

        std::cout << "\n"
                     "╔════════════════════════════════════════════════╗\n";
        std::cout << "║  Abseil: " << std::setw(37) << std::left << title << " ║\n";
        std::cout << "╠════════════════════════════════════════════════╣\n";
        std::cout << std::right;
        std::cout << "║ Threads:           " << std::setw(26) << result.thread_count << "  ║\n";
        std::cout << "║ Iterations/thread: " << std::setw(26) << result.iterations << "  ║\n";
        std::cout << "║ Target record:     " << std::setw(22) << "~256 bytes" << "  ║\n";
        std::cout << "╠════════════════════════════════════════════════╣\n";
        std::cout << "║ Wall-clock time:   " << std::setw(20) << std::fixed << std::setprecision(3) << wall_sec << "s"
                  << "     ║\n";
        std::cout << "║ Throughput:        " << std::setw(20) << std::fixed << std::setprecision(0)
                  << result.entries_per_second << " ops/s" << " ║\n";
        std::cout << "║ Avg thread time:   " << std::setw(20) << std::fixed << std::setprecision(3) << avg_sec << "s"
                  << "     ║\n";
        std::cout << "║ Min thread time:   " << std::setw(20) << std::fixed << std::setprecision(3) << min_sec << "s"
                  << "     ║\n";
        std::cout << "║ Max thread time:   " << std::setw(20) << std::fixed << std::setprecision(3) << max_sec << "s"
                  << "     ║\n";
        std::cout << "╚════════════════════════════════════════════════╝\n";
    }

    BenchmarkResult
    run_benchmark(const std::size_t thread_count, const std::string& padding, const std::string& log_filename) {
        std::filesystem::remove(log_filename);

        AbseilFileSink sink{log_filename};
        absl::AddLogSink(&sink);

        std::barrier sync_point{static_cast<std::ptrdiff_t>(thread_count)};
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < ITERATIONS_PER_THREAD; ++j) {
                    LOG(INFO) << padding;
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

        absl::RemoveLogSink(&sink);
        sink.Flush();

        const auto wall_clock    = wall_end - wall_start;
        const double wall_sec    = std::chrono::duration<double>(wall_clock).count();
        const auto total_entries = static_cast<double>(thread_count * ITERATIONS_PER_THREAD);

        return BenchmarkResult{
            .thread_count       = thread_count,
            .iterations         = ITERATIONS_PER_THREAD,
            .thread_results     = std::move(thread_results),
            .wall_clock         = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock),
            .entries_per_second = total_entries / wall_sec,
        };
    }

    BenchmarkResult run_null_benchmark(const std::size_t thread_count, const std::string& padding) {
        AbseilNullSink sink;
        absl::AddLogSink(&sink);

        std::barrier sync_point{static_cast<std::ptrdiff_t>(thread_count)};
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < ITERATIONS_PER_THREAD; ++j) {
                    LOG(INFO) << padding;
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
        absl::RemoveLogSink(&sink);

        const auto wall_clock    = wall_end - wall_start;
        const double wall_sec    = std::chrono::duration<double>(wall_clock).count();
        const auto total_entries = static_cast<double>(thread_count * ITERATIONS_PER_THREAD);

        return BenchmarkResult{
            .thread_count       = thread_count,
            .iterations         = ITERATIONS_PER_THREAD,
            .thread_results     = std::move(thread_results),
            .wall_clock         = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_clock),
            .entries_per_second = total_entries / wall_sec,
        };
    }

}  // namespace

int main() {
    absl::InitializeLog();
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);

    const std::string padding(ABSEIL_PAD_SIZE, '0');

    std::cout << "Abseil File Logger Benchmark\n";
    std::cout << "Padding size: " << ABSEIL_PAD_SIZE << " bytes (target record ~" << TARGET_RECORD_SIZE << " bytes)\n";

    // Test 1: 8-thread contention
    auto result_8t = run_benchmark(CONTENTION_THREADS, padding, "abseil_contention_8t.log");
    print_results(result_8t, "8-Thread Contention");

    // Test 2: 1-thread baseline
    auto result_1t = run_benchmark(BASELINE_THREADS, padding, "abseil_baseline_1t.log");
    print_results(result_1t, "1-Thread Baseline");

    // Test 3: 8-thread null sink (pure Abseil overhead)
    auto null_8t = run_null_benchmark(CONTENTION_THREADS, padding);
    print_results(null_8t, "8-Thread NullSink");

    // Test 4: 1-thread null sink
    auto null_1t = run_null_benchmark(BASELINE_THREADS, padding);
    print_results(null_1t, "1-Thread NullSink");

    return 0;
}
