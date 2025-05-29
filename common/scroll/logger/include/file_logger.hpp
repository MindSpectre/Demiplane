#pragma once

#include <fstream>
#include <string>

#include "../logger_interface.hpp"

namespace demiplane::scroll {

    template <IsEntry EntryType>
    class FileLogger final : public Logger<EntryType> {
    public:
        explicit FileLogger(const std::string& filename) {
            file_stream_.open(filename, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + filename);
            }
        }

        ~FileLogger() override {
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
        }

        void log(LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function) override {
            // Skip logging if below the threshold
            if (static_cast<int>(level) < static_cast<int>(this->threshold_)) {
                return;
            }

            EntryType entry(level, message, file, line, function);
            file_stream_ << entry.to_string() << std::endl;
        }
        void log(EntryType entry) override {
            if (static_cast<int>(entry.level()) < static_cast<int>(this->threshold_)) {
                return;
            }
            file_stream_ << entry.to_string() << std::endl;
        }

    private:
        std::ofstream file_stream_;
    };

} // namespace demiplane::scroll
