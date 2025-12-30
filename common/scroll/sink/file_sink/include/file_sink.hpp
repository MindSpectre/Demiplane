#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>

#include "file_sink_config.hpp"

namespace demiplane::scroll {

    /**
     * @brief File sink with automatic rotation
     *
     * @tparam EntryType Defines which metadata to include (e.g., DetailedEntry, LightEntry)
     *
     * Features:
     * - Automatic file rotation when max_file_size is reached
     * - Timestamp-based file naming (app_2025-01-18T10:30:45.log)
     * - Thread-safe writing
     * - Automatic directory creation
     *
     * Rotation strategy:
     * When app.log reaches max_file_size, a new file is created with current timestamp:
     *   - app_2025-01-18T10:00:00.log (old file, closed)
     *   - app_2025-01-18T12:30:45.log (new file, active)
     *
     */
    template <detail::EntryConcept EntryType>
    class FileSink final : public Sink {
    public:
        template <typename FileSinkConfigTp = FileSinkConfig>
            requires std::constructible_from<FileSinkConfig, FileSinkConfigTp>
        explicit FileSink(FileSinkConfigTp&& cfg = {}) noexcept
            : config_{std::forward<FileSinkConfigTp>(cfg)} {
            init();
        }

        ~FileSink() override {
            std::lock_guard lock{mutex_};
            if (file_stream_.is_open()) {
                file_stream_.flush();
                file_stream_.close();
            }
        }

        void process(const LogEvent& event) override {
            if (!should_log(event.level)) {
                return;
            }

            // Convert LogEvent → EntryType using existing factory pattern

            // Lock only for the actual write (supports multiple loggers sharing this sink)
            {
                EntryType entry             = make_entry_from_event<EntryType>(event);
                const std::string formatted = entry.to_string();
                std::lock_guard lock{mutex_};
                file_stream_ << formatted;

                if (config_.is_flush_each_entry()) {
                    file_stream_.flush();
                }
            }

            // Check if rotation needed (outside lock to minimize contention)
            if (should_rotate()) {
                rotate_log();
            }
        }

        void flush() override {
            std::lock_guard lock{mutex_};
            file_stream_.flush();
        }

        [[nodiscard]] bool should_log(LogLevel lvl) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.get_threshold());
        }

        constexpr FileSinkConfig& config() noexcept {
            return config_;
        }

        [[nodiscard]] constexpr const FileSinkConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] const std::filesystem::path& file_path() const noexcept {
            return file_path_;
        }

    private:
        FileSinkConfig config_;
        std::ofstream file_stream_;
        std::filesystem::path file_path_;
        std::mutex mutex_;
        alignas(64) char stream_buffer_[64 * 1024]{};  // 64KB static buffer, cache-line aligned

        void init() {
            std::filesystem::path full_path = config_.get_file();

            if (config_.is_add_time_to_filename()) {
                const std::string stem             = full_path.stem().string();
                const std::string ext              = full_path.extension().string();
                const std::filesystem::path parent = full_path.parent_path();
                const std::string time             = chrono::LocalClock::current_time(config_.get_time_format_in_file_name());
                full_path                          = parent / (stem + "_" + time + ext);
            }

            if (!full_path.parent_path().empty()) {
                std::filesystem::create_directories(full_path.parent_path());
            }

            file_stream_.open(full_path, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error{"Failed to open log file: " + full_path.string()};
            }

            // Configure ofstream buffer for better batching of syscalls
            file_stream_.rdbuf()->pubsetbuf(stream_buffer_, sizeof(stream_buffer_));
            file_path_ = full_path;
        }

        bool should_rotate() {
            if (!config_.is_add_time_to_filename()) {
                return false;  // Can't rotate to same file
            }
            std::uint64_t sz = 0;
            if (const auto pos = file_stream_.tellp(); pos >= 0) {
                sz = static_cast<std::uint64_t>(pos);
            } else {
                try {
                    sz = std::filesystem::file_size(config_.get_file());
                } catch (...) {
                    sz = 0;
                }
            }
            return sz > config_.get_max_file_size();
        }

        void rotate_log() {
            std::lock_guard lock{mutex_};
            file_stream_.flush();
            file_stream_.close();
            init();
        }
    };
}  // namespace demiplane::scroll
