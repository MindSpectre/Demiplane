#pragma once

#include <fstream>
#include <string>
#include <utility>

#include "../logger_interface.hpp"
namespace demiplane::scroll {

    template <detail::EntryConcept EntryType>
    class FileLogger final : public Logger<EntryType> {
    public:
        explicit FileLogger(std::string filename) : filename_(std::move(filename)) {
            init();
        }

        FileLogger(std::string filename, LogLevel threshold)
            : Logger<EntryType>{threshold}, filename_(std::move(filename)) {
            init();
        }

        ~FileLogger() override {
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
        }

        void log(const LogLevel lvl, const std::string_view msg, const std::source_location loc) override {
            auto entry = make_entry<EntryType>(lvl, msg, loc);
            log(entry);
        }

        void log(const EntryType& entry) override {
            if (static_cast<int8_t>(entry.level()) < static_cast<int8_t>(this->threshold_)) {
                return;
            }
            if (safe_mode_) {
                file_stream_.close();
                init();
            }
            file_stream_ << entry.to_string();
            if (safe_mode_) {
                file_stream_.flush();
            }
        }
        void set_file(const std::string& filename) {
            init(filename);
        }
        void set_safe_mode() {
            safe_mode_ = true;
        }
        void flush() {
            file_stream_.flush();
        }

    private:
        void init() {
            // TODO: Create file creation for nested dirs
            file_stream_.open(filename_, std::ios::out | std::ios::app);
            if (!file_stream_.is_open()) {
                throw std::runtime_error("Failed to open log file: " + filename_);
            }
        }
        bool safe_mode_ = false;
        std::string filename_;
        std::ofstream file_stream_;
    };

} // namespace demiplane::scroll
