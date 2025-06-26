#pragma once

#include <atomic>
#include <condition_variable>
#include <demiplane/chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../logger_interface.hpp"
#include "gears_types.hpp"
#include <concurrent_queue.hpp> // moodycamel
namespace demiplane::scroll {
    struct FileLoggerConfig {
        LogLevel threshold{LogLevel::Debug};
        std::filesystem::path file;
        bool add_time_to_name{true};
        bool safe_mode{false}; // extreme slow
        uint64_t max_file_size = gears::literals::operator""_mb(100); // mb
    };


    template <detail::EntryConcept EntryType>
    class FileLogger final : public Logger<EntryType> {
    public:
        explicit FileLogger(FileLoggerConfig cfg) : config_(std::move(cfg)), running_(true) {
            init(); // open first file
            worker_ = std::thread(&FileLogger::writerLoop, this);
        }

        ~FileLogger() override {
            running_.store(false, std::memory_order_release);
            cv_.notify_one();
            if (worker_.joinable()) {
                worker_.join(); // drain & flush
            }
            file_stream_.close();
        }

        /*------------------------------------------------------------*/

        void log(const LogLevel lvl, const std::string_view msg, const std::source_location loc) override {
            EntryType entry = make_entry<EntryType>(lvl, msg, loc);
            log(entry);
        }

        void log(const EntryType& entry) override {
            if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(config_.threshold)) {
                return;
            }

            enqueue(entry.to_string());
        }

        FileLoggerConfig& config() {
            return config_;
        }
        void reload() {
            { // mark send sentinel
                std::unique_lock lk(reload_mtx_);
                reload_done_ = false;
                reload_requested_.store(true, std::memory_order_release);
            }
            queue_.enqueue(kReloadSentinel); // wake writer deterministically
            std::unique_lock lk(reload_mtx_);
            reload_cv_.wait(lk, [&] { return reload_done_; });
        }

    private:
        /* ====  Hot-path helper  =================================== */
        void enqueue(std::string text) noexcept {
            queue_.enqueue(std::move(text)); // lock-free
            cv_.notify_one(); // wake consumer
        }

        /* ====  Writer thread  ===================================== */
        void writerLoop() {
            std::vector<std::string> batch;
            batch.reserve(512);

            while (running_.load(std::memory_order_acquire)) {
                // 1) Pull everything that’s available
                std::string item;

                while (queue_.try_dequeue(item)) {
                    std::cout << item << std::endl;
                    if (item == kReloadSentinel) {
                        break;
                    }
                    batch.emplace_back(std::move(item));
                }
                if (reload_requested_.load(std::memory_order_acquire)) // old file empty
                {
                    file_stream_.flush();
                    file_stream_.close();
                    init(); // uses *updated* config_
                    reload_requested_.store(false, std::memory_order_release);

                    {
                        std::lock_guard lg(reload_mtx_);
                        reload_done_ = true; // handshake
                    }
                    reload_cv_.notify_one();
                    continue;
                }
                // 2) If nothing, wait a bit (avoids busy-spin)
                if (batch.empty()) {
                    std::unique_lock lk(m_);
                    cv_.wait_for(lk, std::chrono::milliseconds(100), [&] { return !running_.load(); });
                    continue; // nothing else to do this round
                }

                // 3) Write batch
                for (auto& line : batch) {
                    file_stream_ << line;
                }
                if (config_.safe_mode) {
                    file_stream_.flush();
                }
                batch.clear();

                // 4) Rotation check (do it rarely, O(1))

                std::uint64_t sz = 0;

                /* 1. cheap & safe: how many bytes have we written? */
                if (auto pos = file_stream_.tellp(); pos >= 0) {
                    sz = static_cast<std::uint64_t>(pos);
                } else {
                    /* 2. fall back to path (may be missing) */
                    try {
                        sz = std::filesystem::file_size(config_.file);
                    } catch (const std::filesystem::filesystem_error&) {
                        sz = 0; // path vanished – treat as 0
                    }
                }

                if (sz > config_.max_file_size) {
                    rotateLog();
                }

                /*---- 5) handle requested reload ---------------------*/
            }
            // Drain done – final flush
            file_stream_.flush();
        }

        /* ====  Rotation =========================================== */
        void rotateLog() {
            file_stream_.flush();
            file_stream_.close();
            init(); // opens a new file
        }

        /* ====  init() exactly as you already have ================= */
        void init() try {
            // Clone original path to work on a local mutable version
            auto full_path = config_.file;

            if (config_.add_time_to_name) {
                // Strip directory and extension
                const auto stem   = full_path.stem().string(); // e.g., "file"
                const auto ext    = full_path.extension().string(); // e.g., ".log"
                const auto parent = full_path.parent_path(); // e.g., "/path/to"

                // Format time and construct final file name
                const auto time =
                    chrono::LocalClock::current_time_fmt(chrono::clock_formats::dmy_hms); // e.g., "25_06_2025_23_40_01"
                const std::string new_filename = stem + "_" + time + ext; // "file_25_06_2025_23_40_01.log"

                // Replace only the filename — not the full path
                full_path = parent / new_filename;
            }

            // Ensure parent directory exists
            if (!full_path.parent_path().empty()) {
                create_directories(full_path.parent_path());
            }

            // Open file stream
            file_stream_.open(full_path, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + full_path.string());
            }

            // If successful, update internal config file path (not original one)
            config_.file = full_path;
        } catch (const std::exception& ex) {
            std::cerr << "[Logger Init Error] " << ex.what() << std::endl;
            throw; // optionally rethrow or handle gracefully
        }

        /* ====  Data =============================================== */
        FileLoggerConfig config_;
        std::ofstream file_stream_;

        // ---- concurrency primitives ----
        moodycamel::ConcurrentQueue<std::string> queue_;
        // boost::lockfree::queue<std::string> queue_{1024};
        std::atomic<bool> reload_requested_{false};
        std::atomic<bool> running_{false};
        std::thread worker_;
        std::condition_variable cv_;
        std::mutex m_; // only for cv_

        std::mutex reload_mtx_;
        std::condition_variable reload_cv_;
        bool reload_done_{false};

        static constexpr auto kReloadSentinel = "BREAK"; // possible vuln
    };


} // namespace demiplane::scroll
