#include <demiplane/math>
#include <demiplane/nexus>
#include <demiplane/scroll>
#include <iostream>
#include <memory>
#include <source_location>
#include <thread>

#include <printing_stopwatch.hpp>

inline std::chrono::milliseconds parse_sec_ms(std::string_view line) {
    // indexes for "… HH:MM:SS.mmmZ"
    //            012345678901234567890123
    //            ----------^  ^   ^
    //            pos 17      20  23 (Z)

    if (line.size() < 23) {
        return std::chrono::milliseconds{0};
    }

    const int sec = std::stoi(std::string{line.substr(17, 2)});
    const int ms  = std::stoi(std::string{line.substr(20, 3)});

    return std::chrono::seconds{sec} + std::chrono::milliseconds{ms};
}

template <typename T>
void multithread_write(const T& file_logger) {
    std::vector<std::thread> threads;
    // Launch multiple threads to acquire and release objects
    std::size_t t_num = 10;
    std::size_t r_num = 10'000;
    std::chrono::nanoseconds process_time{50};
    demiplane::math::random::RandomTimeGenerator time_generator;
    demiplane::gears::unused_value(process_time);
    threads.reserve(t_num);
    demiplane::chrono::PrintingStopwatch<> twp;
    twp.start();
    for (std::size_t i = 0; i < t_num; ++i) {
        threads.emplace_back([&] {
            for (std::size_t j = 0; j < r_num; ++j) {
                SCROLL_LOG_DIRECT_STREAM_DBG(file_logger.get()) << "asdasd";
                std::this_thread::sleep_for(process_time);
            }
        });
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }
    file_logger->graceful_shutdown();
    twp.finish();
    std::ifstream in(file_logger->file_path());
    if (!in.is_open()) {
        std::cout << "File not found" << '\n';
    }
    std::string line;
    std::chrono::milliseconds prev{};
    bool first                     = true;
    std::uint32_t monotonic_errors = 0;  // how many times we go backwards?
    std::uint32_t total_lines      = 0;
    std::string prevl;
    while (std::getline(in, line)) {
        auto ts = parse_sec_ms(line);

        if (!first) {
            if (ts < prev) {
                // std::cout << "Non-monotonic line: " << prevl << "\n" << line << '\n';
                ++monotonic_errors;  // or store the offending line
                std::cout << total_lines << '\n';
            }
        } else {
            first = false;
        }
        prev = ts;
        // prevl = line;
        total_lines++;
    }

    std::cout << "Non-monotonic lines: " << monotonic_errors << " " << total_lines << '\n';
    std::cout << "Non-monotonic lines%: "
              << static_cast<double>(100 * monotonic_errors) / (static_cast<double>(t_num) * static_cast<double>(r_num))
              << '\n';
}

template <typename T>
void unsafe_write(const T& file_logger) {
    file_logger->config().sort_entries = false;

    multithread_write(file_logger);
}

template <typename T>
void safe_write(const T& file_logger) {
    file_logger->config().sort_entries = true;
    file_logger->config().batch_size   = 1 << 10;
    // TODO: result out of order between batches
    file_logger->reload();
    multithread_write(file_logger);
}

int main() {
    demiplane::scroll::FileLoggerConfig cfg{.threshold            = demiplane::scroll::DBG,
                                            .file                 = "test.log",
                                            .add_time_to_filename = false,
                                            .sort_entries         = true,
                                            .flush_each_batch     = true};
    std::filesystem::remove(cfg.file);
    const auto file_logger = std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(cfg);
    // safe_write(file_logger);
    unsafe_write(file_logger);
    return 0;
}
