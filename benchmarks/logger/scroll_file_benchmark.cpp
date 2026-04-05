#include <barrier>
#include <chrono>
#include <demiplane/scroll>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <source_location>
#include <thread>
#include <vector>

namespace {

    // Null sink: accepts events, does nothing. Measures pure disruptor overhead.
    class NullSink final : public demiplane::scroll::Sink {
    public:
        void process(const demiplane::scroll::LogEvent& /*event*/) override {
        }
        void flush() override {
        }
        [[nodiscard]] bool should_log(demiplane::scroll::LogLevel /*lvl*/) const noexcept override {
            return true;
        }
    };

    constexpr std::size_t CONTENTION_THREADS    = 2;
    constexpr std::size_t BASELINE_THREADS      = 1;
    constexpr std::size_t ITERATIONS_PER_THREAD = 1'000'000;

    // Estimated Scroll DetailedEntry prefix: ~96 bytes
    // Format: "2026-04-05T17:17:23.835Z INF [scroll_file_benchmark.cpp:XX] [tid NNNNNNNNNNNNNNN, pid NNNNNN] "
    constexpr std::size_t TARGET_RECORD_SIZE = 256;
    constexpr std::size_t SCROLL_PREFIX_EST  = 104;
    constexpr std::size_t SCROLL_PAD_SIZE    = TARGET_RECORD_SIZE - SCROLL_PREFIX_EST;

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
        std::cout << "║  Scroll: " << std::setw(37) << std::left << title << " ║\n";
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

        demiplane::scroll::FileSinkConfig sink_config =
            demiplane::scroll::FileSinkConfig{}
                .threshold(demiplane::scroll::LogLevel::Debug)
                .file(log_filename)
                .add_time_to_filename(false)
                .max_file_size(demiplane::gears::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotation(false)
                .finalize();

        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(sink_config);

        auto logger = std::make_unique<demiplane::scroll::Logger>(
            demiplane::scroll::LoggerConfig{}
                .wait_strategy<demiplane::scroll::LoggerConfig::WaitStrategy::BusySpin>()
                .ring_buffer_size(demiplane::scroll::LoggerConfig::BufferCapacity::Medium)
                .finalize());
        logger->add_sink(file_sink);

        std::barrier sync_point{static_cast<std::ptrdiff_t>(thread_count)};
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < ITERATIONS_PER_THREAD; ++j) {
                    logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
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
        logger->shutdown();

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
        auto null_sink = std::make_shared<NullSink>();

        auto logger = std::make_unique<demiplane::scroll::Logger>(
            demiplane::scroll::LoggerConfig{}
                .wait_strategy<demiplane::scroll::LoggerConfig::WaitStrategy::BusySpin>()
                .ring_buffer_size(demiplane::scroll::LoggerConfig::BufferCapacity::Medium)
                .finalize());
        logger->add_sink(null_sink);

        std::barrier sync_point{static_cast<std::ptrdiff_t>(thread_count)};
        std::vector<ThreadResult> thread_results(thread_count);
        std::vector<std::thread> threads;
        threads.reserve(thread_count);

        const auto wall_start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&, id = i] {
                const auto thread_start = std::chrono::steady_clock::now();

                for (std::size_t j = 0; j < ITERATIONS_PER_THREAD; ++j) {
                    logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
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
        logger->shutdown();

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
    const std::string padding(SCROLL_PAD_SIZE, '0');

    std::cout << "Scroll File Logger Benchmark\n";
    std::cout << "Padding size: " << SCROLL_PAD_SIZE << " bytes (target record ~" << TARGET_RECORD_SIZE << " bytes)\n";

    // Test 1: 8-thread contention
    auto result_8t = run_benchmark(CONTENTION_THREADS, padding, "scroll_contention_8t.log");
    print_results(result_8t, "8-Thread Contention");

    // Test 2: 1-thread baseline
    auto result_1t = run_benchmark(BASELINE_THREADS, padding, "scroll_baseline_1t.log");
    print_results(result_1t, "1-Thread Baseline");

    // Test 3: 8-thread null sink (pure disruptor overhead)
    auto null_8t = run_null_benchmark(CONTENTION_THREADS, padding);
    print_results(null_8t, "8-Thread NullSink");

    // Test 4: 1-thread null sink
    auto null_1t = run_null_benchmark(BASELINE_THREADS, padding);
    print_results(null_1t, "1-Thread NullSink");

    return 0;
}
