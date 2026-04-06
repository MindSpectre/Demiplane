#include <filesystem>
#include <fstream>
#include <mutex>

#include <absl/log/globals.h>
#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/log_sink.h>
#include <absl/log/log_sink_registry.h>

#include "benchmark_harness.hpp"

namespace {

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

    constexpr std::size_t ABSEIL_PREFIX_EST = 70;
    constexpr std::size_t ABSEIL_PAD_SIZE   = bench::TARGET_RECORD_SIZE - ABSEIL_PREFIX_EST;

}  // namespace

int main() {
    absl::InitializeLog();
    absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfinity);
    absl::SetMinLogLevel(absl::LogSeverityAtLeast::kInfo);

    const std::string padding(ABSEIL_PAD_SIZE, '0');

    std::cout << "Abseil File Logger Benchmark\n";
    std::cout << "Padding size: " << ABSEIL_PAD_SIZE << " bytes (target record ~" << bench::TARGET_RECORD_SIZE
              << " bytes)\n";

    // Test 1: 8-thread file sink contention
    {
        std::filesystem::remove("abseil_contention_8t.log");
        AbseilFileSink sink{"abseil_contention_8t.log"};
        absl::AddLogSink(&sink);

        auto result = bench::run_threaded_benchmark(
            bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] { LOG(INFO) << padding; });
        absl::RemoveLogSink(&sink);
        sink.Flush();
        bench::print_results(result, "Abseil", "8-Thread Contention");
    }

    // Test 2: 1-thread file sink baseline
    {
        std::filesystem::remove("abseil_baseline_1t.log");
        AbseilFileSink sink{"abseil_baseline_1t.log"};
        absl::AddLogSink(&sink);

        auto result = bench::run_threaded_benchmark(
            bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] { LOG(INFO) << padding; });
        absl::RemoveLogSink(&sink);
        sink.Flush();
        bench::print_results(result, "Abseil", "1-Thread Baseline");
    }

    // Test 3: 8-thread null sink
    {
        AbseilNullSink sink;
        absl::AddLogSink(&sink);

        auto result = bench::run_threaded_benchmark(
            bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] { LOG(INFO) << padding; });
        absl::RemoveLogSink(&sink);
        bench::print_results(result, "Abseil", "8-Thread NullSink");
    }

    // Test 4: 1-thread null sink
    {
        AbseilNullSink sink;
        absl::AddLogSink(&sink);

        auto result = bench::run_threaded_benchmark(
            bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] { LOG(INFO) << padding; });
        absl::RemoveLogSink(&sink);
        bench::print_results(result, "Abseil", "1-Thread NullSink");
    }

    // Test 5: 8-thread dual file sink
    {
        std::filesystem::remove("abseil_dual_a.log");
        std::filesystem::remove("abseil_dual_b.log");
        AbseilFileSink sink_a{"abseil_dual_a.log"};
        AbseilFileSink sink_b{"abseil_dual_b.log"};
        absl::AddLogSink(&sink_a);
        absl::AddLogSink(&sink_b);

        auto result = bench::run_threaded_benchmark(
            bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] { LOG(INFO) << padding; });
        absl::RemoveLogSink(&sink_a);
        absl::RemoveLogSink(&sink_b);
        sink_a.Flush();
        sink_b.Flush();
        bench::print_results(result, "Abseil", "8-Thread Dual FileSink");
    }

    return 0;
}
