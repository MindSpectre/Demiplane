#pragma once

#include <queue>

#include "../tracer_interface.hpp"

namespace demiplane::scroll {
    template <class Service>
    class FileTracer final : public TracerInterface<Service> {
    public:
        ~FileTracer() override {
            std::lock_guard lock(mutex_);
            if (log_file_.is_open()) {
                log_file_.flush();
                log_file_.close();
            }
        }

        // Logs a message to the current log file.
        void log(LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function) override {
            // Thread-local flag to ensure we print the header once per thread.

            // Check against the threshold.
            if (static_cast<int>(level) < static_cast<int>(config_->get_threshold())) {
                return;
            }

            std::lock_guard lock(mutex_);

            // If header is enabled and not yet written for this thread, write it.
            if (thread_local bool header_written = false; this->processor_.config().enable_header && !header_written) {
                if (log_file_.is_open()) {
                    log_file_ << this->processor_.make_header() << "\n";
                    header_written = true;
                }
            }

            // Create the log entry.
            const std::string entry = this->processor_.create_entry(level, message, file, line, function, Service::name());

            // If no file is open, create a default one.
            if (!log_file_.is_open()) {
                new_file();
            }

            // Write the entry.
            log_file_ << entry << "\n";
            ++record_count_;

            // Rotate file if we reached the maximum record count.
            if (record_count_ >= record_max_count_) {
                new_file();
            }
        }

        // Creates/opens a new log file with a custom name appended to the directory path.
        void set_file(const std::string_view file_name) {
            std::lock_guard lock(mutex_);
            if (log_file_.is_open()) {
                log_file_.flush();
                log_file_.close();
            }
            const std::string full_path = directory_path_ + std::string(file_name);
            log_file_.open(full_path, std::ios::out | std::ios::app);
            record_count_ = 0;
            if (!log_file_) {
                std::cerr << "Failed to open log file: " << full_path << "\n";
            }
        }

        // Creates/opens a new log file with a default name (using a timestamp).
        void new_file() {
            set_file(create_log_file_name(directory_path_));
        }
        explicit FileTracer(std::shared_ptr<FileTracerConfig> config) : TracerInterface<Service>(config), config_(std::move(config)) {}

    private:
        std::shared_ptr<FileTracerConfig> config_;
        static std::string create_log_file_name(const std::string_view directory_path) {
            std::ostringstream oss;
            oss << directory_path << "log_" << utilities::chrono::LocalClock::current_time_dmy() << ".log";
            return oss.str();
        }
        std::ofstream log_file_; // The current log file.
        std::string directory_path_ = "./logs/"; // Default directory for logs.
        std::mutex mutex_; // Mutex for thread-safe logging.
        uint32_t record_max_count_ = 1 << 20; // Maximum records per file before rotating.
        uint32_t record_count_     = 0; // Current count of log records.
    };

} // namespace demiplane::tracing
