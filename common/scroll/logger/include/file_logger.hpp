#pragma once

#include <fstream>
#include <string>

#include "../logger_interface.hpp"

namespace demiplane::scroll {

    template <IsEntry EntryType>
    class FileLogger final : public Logger<EntryType> {
    public:
        explicit FileLogger(const std::string& filename) {
            init(filename);
        }

        FileLogger(const std::string& filename, LogLevel threshold) : Logger<EntryType>{threshold} {
            init(filename);
        }

        ~FileLogger() override {
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
        }

        void log(LogLevel lvl, std::string_view msg, std::source_location loc) override {
            if (static_cast<int>(lvl) < static_cast<int>(this->threshold_)) {
                return;
            }
            auto entry = make_entry<EntryType>(lvl, msg, loc);
            file_stream_ << entry.to_string();
        }

        void log(const EntryType& entry) override {
            if (static_cast<int>(entry.level()) < static_cast<int>(this->threshold_)) {
                return;
            }
            file_stream_ << entry.to_string();
        }

    private:
        void init(const std::string& filename) {
            file_stream_.open(filename, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + filename);
            }
        }

        std::ofstream file_stream_;
    };

} // namespace demiplane::scroll
