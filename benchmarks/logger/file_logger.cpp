#include <demiplane/scroll>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <source_location>
#include <thread>
#include <vector>

#include <stopwatch.hpp>

namespace {

    constexpr std::size_t THREAD_COUNT           = 4;
    constexpr std::size_t RECORDS_PER_THREAD     = 1'000'000;
    constexpr std::size_t TOTAL_EXPECTED_RECORDS = THREAD_COUNT * RECORDS_PER_THREAD;

    struct BenchmarkResult {
        std::size_t total_entries        = 0;
        std::chrono::nanoseconds elapsed = std::chrono::nanoseconds{0};
        double entries_per_second        = 0.0;
        double avg_latency_ns            = 0.0;
    };

    BenchmarkResult count_log_entries(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open log file: " << filename << '\n';
            return {};
        }

        std::size_t count = 0;
        std::string line;
        while (std::getline(file, line)) {
            count++;
        }

        return {.total_entries = count};
    }

    void print_benchmark_results(const BenchmarkResult& result) {
        std::cout << "\n"
                     "╔════════════════════════════════════════╗\n";
        std::cout << "║     File Logger Benchmark Results      ║\n";
        std::cout << "╠════════════════════════════════════════╣\n";
        std::cout << "║ Threads:           " << std::setw(18) << THREAD_COUNT << "  ║\n";
        std::cout << "║ Entries/thread:    " << std::setw(18) << RECORDS_PER_THREAD << "  ║\n";
        std::cout << "║ Total entries:     " << std::setw(18) << TOTAL_EXPECTED_RECORDS << "  ║\n";
        std::cout << "╠════════════════════════════════════════╣\n";
        std::cout << "║ Elapsed time:      " << std::setw(10) << std::fixed << std::setprecision(3)
                  << static_cast<double>(result.elapsed.count()) / 1e9 << "s    ║\n";
        std::cout << "║ Throughput:        " << std::setw(10) << std::fixed << std::setprecision(0)
                  << result.entries_per_second << " ops/s ║\n";
        std::cout << "║ Avg latency:       " << std::setw(10) << std::fixed << std::setprecision(2)
                  << result.avg_latency_ns << " ns   ║\n";
        std::cout << "╠════════════════════════════════════════╣\n";
        std::cout << "║ Entries logged:    " << std::setw(15) << result.total_entries << " ║\n";

        if (result.total_entries == TOTAL_EXPECTED_RECORDS) {
            std::cout << "║ Status:            " << std::setw(15) << "PASSED ✓" << " ║\n";
        } else {
            std::cout << "║ Status:            " << std::setw(15) << "FAILED ✗" << " ║\n";
            std::cout << "║ Missing:           " << std::setw(15) << (TOTAL_EXPECTED_RECORDS - result.total_entries)
                      << " ║\n";
        }

        std::cout << "╚════════════════════════════════════════╝\n";
    }

}  // namespace

void run_throughput_benchmark(
    const std::shared_ptr<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>& sink) {
    auto logger = std::make_unique<demiplane::scroll::Logger>(
        demiplane::scroll::LoggerConfig{}
            .wait_strategy<demiplane::scroll::LoggerConfig::WaitStrategy::BusySpin>()
            .ring_buffer_size(demiplane::scroll::LoggerConfig::BufferCapacity::Medium)
            .finalize());
    logger->add_sink(sink);

    std::vector<std::thread> threads;
    threads.reserve(THREAD_COUNT);


    const auto elapsed = demiplane::chrono::Stopwatch<std::chrono::nanoseconds>::measure([&threads, &logger] {
        // Launch worker threads - each logs as fast as possible
        for (std::size_t i = 0; i < THREAD_COUNT; ++i) {
            threads.emplace_back([&logger, thread_id = i] {
                for (std::size_t j = 0; j < RECORDS_PER_THREAD; ++j) {
                    logger->log(demiplane::scroll::LogLevel::Debug,
                                "Thread {} iteration {} - benchmark message",
                                std::source_location::current(),
                                thread_id,
                                j);
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
    });
    logger->shutdown();
    // Count entries in file
    auto result    = count_log_entries(sink->config().get_file());
    result.elapsed = elapsed;

    // Calculate throughput and latency
    const double elapsed_seconds = static_cast<double>(elapsed.count()) / 1e9;
    result.entries_per_second    = static_cast<double>(result.total_entries) / elapsed_seconds;
    result.avg_latency_ns        = static_cast<double>(elapsed.count()) / static_cast<double>(result.total_entries);

    print_benchmark_results(result);
}

int main() {
    std::cout << "Starting File Logger Throughput Benchmark...\n";
    std::cout << "Configuration: " << THREAD_COUNT << " threads × " << RECORDS_PER_THREAD
              << " entries = " << TOTAL_EXPECTED_RECORDS << " total\n";

    demiplane::scroll::FileSinkConfig config = demiplane::scroll::FileSinkConfig{}
                                                   .threshold(demiplane::scroll::LogLevel::Debug)
                                                   .file("benchmark_throughput.log")
                                                   .add_time_to_filename(false)
                                                   .max_file_size(demiplane::gears::literals::operator""_mb(500))
                                                   .flush_each_entry(false)
                                                   .rotation(false)
                                                   .finalize();  // Maximum throughput mode

    std::filesystem::remove(config.get_file());

    const auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(config);

    run_throughput_benchmark(file_sink);

    std::cout << "\nLog file: " << config.get_file() << "\n";

    return 0;
}
