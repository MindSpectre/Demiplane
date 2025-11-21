#pragma once

#include <demiplane/chrono>
#include <filesystem>
#include <fstream>
#include <mutex>

#include <sink_interface.hpp>

namespace demiplane::scroll {
    /**
     * @brief Configuration for file output with rotation
     */
    struct FileSinkConfig {
        LogLevel threshold = LogLevel::Debug;
        std::filesystem::path file;

        /// @brief Add time to a file name. Current is iso8601
        bool add_time_to_filename = true;
        std::string time_format_in_file_name = chrono::clock_formats::iso8601;

        /// @brief The default size is 100 mb.
        std::uint64_t max_file_size = gears::literals::operator""_mb(100);
    };

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
     * Usage:
     *   auto file = std::make_unique<FileSink<DetailedEntry>>(
     *       FileSinkConfig{
     *           .file = "/var/log/myapp.log",
     *           .max_file_size = gears::literals::operator""_mb(50)
     *       }
     *   );
     *   logger.add_sink(std::move(file));
     */
    template <detail::EntryConcept EntryType>
    class FileSink final : public Sink {
    public:
        explicit FileSink(FileSinkConfig cfg)
            : config_{std::move(cfg)} {
            init();
        }

        ~FileSink() override {
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
        }

        void process(const LogEvent& event) override {
            if (!should_log(event.level)) {
                return;
            }

            // Convert LogEvent → EntryType using existing factory pattern
            EntryType entry = make_entry_from_event<EntryType>(event);
            std::string formatted = entry.to_string();

            std::lock_guard lock{mutex_};

            file_stream_ << formatted;

            // Check if rotation needed
            if (should_rotate()) {
                rotate_log();
            }
        }

        void flush() override {
            std::lock_guard lock{mutex_};
            file_stream_.flush();
        }

        [[nodiscard]] bool should_log(LogLevel lvl) const noexcept override {
            return static_cast<int8_t>(lvl) >= static_cast<int8_t>(config_.threshold);
        }

        FileSinkConfig& config() noexcept {
            return config_;
        }

        [[nodiscard]] const FileSinkConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] const std::filesystem::path& file_path() const noexcept {
            return file_path_;
        }

    private:
        FileSinkConfig config_;
        std::ofstream file_stream_;
        std::filesystem::path file_path_;
        mutable std::mutex mutex_;

        void init() {
            std::filesystem::path full_path = config_.file;

            if (config_.add_time_to_filename) {
                const std::string stem = full_path.stem().string();
                const std::string ext = full_path.extension().string();
                const std::filesystem::path parent = full_path.parent_path();
                const std::string time = chrono::LocalClock::current_time(config_.time_format_in_file_name);
                full_path = parent / (stem + "_" + time + ext);
            }

            if (!full_path.parent_path().empty()) {
                std::filesystem::create_directories(full_path.parent_path());
            }

            file_stream_.open(full_path, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error{"Failed to open log file: " + full_path.string()};
            }

            file_path_ = full_path;
        }

        bool should_rotate() {
            std::uint64_t sz = 0;
            if (const auto pos = file_stream_.tellp(); pos >= 0) {
                sz = static_cast<std::uint64_t>(pos);
            } else {
                try {
                    sz = std::filesystem::file_size(config_.file);
                } catch (...) {
                    sz = 0;
                }
            }
            return sz > config_.max_file_size;
        }

        void rotate_log() {
            file_stream_.flush();
            file_stream_.close();
            init();
        }
    };
}  // namespace demiplane::scroll
