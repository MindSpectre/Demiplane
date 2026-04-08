#include <demiplane/scroll>
#include <filesystem>
#include <source_location>

#include "benchmark_harness.hpp"

namespace {

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

    constexpr std::size_t SCROLL_PREFIX_EST = 104;
    constexpr std::size_t SCROLL_PAD_SIZE   = bench::TARGET_RECORD_SIZE - SCROLL_PREFIX_EST;

    auto make_logger() {
        return std::make_unique<demiplane::scroll::Logger>(
            demiplane::scroll::LoggerConfig{}
                .wait_strategy<demiplane::scroll::LoggerConfig::WaitStrategy::BusySpin>()
                .ring_buffer_size(demiplane::scroll::LoggerConfig::BufferCapacity::Medium)
                .finalize());
    }

}  // namespace

int main() {
    const std::string padding(SCROLL_PAD_SIZE, '0');

    std::cout << "Scroll File Logger Benchmark\n";
    std::cout << "Padding size: " << SCROLL_PAD_SIZE << " bytes (target record ~" << bench::TARGET_RECORD_SIZE
              << " bytes)\n";

    // Test 1: 8-thread file sink contention
    {
        std::filesystem::remove("scroll_contention_8t.log");

        demiplane::scroll::FileSinkConfig sink_config =
            demiplane::scroll::FileSinkConfig{}
                .threshold(demiplane::scroll::LogLevel::Debug)
                .file("scroll_contention_8t.log")
                .add_time_to_filename(false)
                .max_file_size(demiplane::gears::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotation(false)
                .finalize();

        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(sink_config);
        auto logger    = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread Contention");
    }

    // Test 2: 1-thread file sink baseline
    {
        std::filesystem::remove("scroll_baseline_1t.log");

        demiplane::scroll::FileSinkConfig sink_config =
            demiplane::scroll::FileSinkConfig{}
                .threshold(demiplane::scroll::LogLevel::Debug)
                .file("scroll_baseline_1t.log")
                .add_time_to_filename(false)
                .max_file_size(demiplane::gears::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotation(false)
                .finalize();

        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(sink_config);
        auto logger    = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark(bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "1-Thread Baseline");
    }

    // Test 3: 8-thread null sink
    {
        auto null_sink = std::make_shared<NullSink>();
        auto logger    = make_logger();
        logger->add_sink(null_sink);

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread NullSink");
    }

    // Test 4: 1-thread null sink
    {
        auto null_sink = std::make_shared<NullSink>();
        auto logger    = make_logger();
        logger->add_sink(null_sink);

        auto result = bench::run_threaded_benchmark(bench::BASELINE_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "1-Thread NullSink");
    }

    // Test 5: 8-thread dual file sink (async parallelism test)
    {
        std::filesystem::remove("scroll_dual_a.log");
        std::filesystem::remove("scroll_dual_b.log");

        auto make_file_sink = [](const std::string& path) {
            return std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(
                demiplane::scroll::FileSinkConfig{}
                    .threshold(demiplane::scroll::LogLevel::Debug)
                    .file(path)
                    .add_time_to_filename(false)
                    .max_file_size(demiplane::gears::literals::operator""_mb(500))
                    .flush_each_entry(false)
                    .rotation(false)
                    .finalize());
        };

        auto logger = make_logger();
        logger->add_sink(make_file_sink("scroll_dual_a.log"));
        logger->add_sink(make_file_sink("scroll_dual_b.log"));

        auto result = bench::run_threaded_benchmark(bench::CONTENTION_THREADS, bench::ITERATIONS_PER_THREAD, [&] {
            logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding);
        });
        logger->shutdown();
        bench::print_results(result, "Scroll", "8-Thread Dual FileSink");
    }

    // Test 6: 8-thread E2E (wall-clock includes shutdown/strand drain)
    {
        std::filesystem::remove("scroll_e2e_8t.log");

        auto file_sink = std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(
            demiplane::scroll::FileSinkConfig{}
                .threshold(demiplane::scroll::LogLevel::Debug)
                .file("scroll_e2e_8t.log")
                .add_time_to_filename(false)
                .max_file_size(demiplane::gears::literals::operator""_mb(500))
                .flush_each_entry(false)
                .rotation(false)
                .finalize());

        auto logger = make_logger();
        logger->add_sink(file_sink);

        auto result = bench::run_threaded_benchmark_e2e(
            bench::CONTENTION_THREADS,
            bench::ITERATIONS_PER_THREAD,
            [&] { logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding); },
            [&] { logger->shutdown(); });
        bench::print_results(result, "Scroll", "8-Thread E2E (incl. shutdown)");
    }

    // Test 7: 8-thread dual file sink E2E (incl. shutdown)
    {
        std::filesystem::remove("scroll_e2e_dual_a.log");
        std::filesystem::remove("scroll_e2e_dual_b.log");

        auto make_file_sink = [](const std::string& path) {
            return std::make_shared<demiplane::scroll::FileSink<demiplane::scroll::DetailedEntry>>(
                demiplane::scroll::FileSinkConfig{}
                    .threshold(demiplane::scroll::LogLevel::Debug)
                    .file(path)
                    .add_time_to_filename(false)
                    .max_file_size(demiplane::gears::literals::operator""_mb(500))
                    .flush_each_entry(false)
                    .rotation(false)
                    .finalize());
        };

        auto logger = make_logger();
        logger->add_sink(make_file_sink("scroll_e2e_dual_a.log"));
        logger->add_sink(make_file_sink("scroll_e2e_dual_b.log"));

        auto result = bench::run_threaded_benchmark_e2e(
            bench::CONTENTION_THREADS,
            bench::ITERATIONS_PER_THREAD,
            [&] { logger->log(demiplane::scroll::LogLevel::Debug, "{}", std::source_location::current(), padding); },
            [&] { logger->shutdown(); });
        bench::print_results(result, "Scroll", "8-Thread Dual E2E (incl. shutdown)");
    }

    return 0;
}
