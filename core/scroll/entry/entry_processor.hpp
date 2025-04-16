#pragma once

#include "colors.hpp"
#include "entry_config.hpp"

namespace demiplane::scroll {
    class EntryProcessor : gears::Immutable {
    public:
        [[nodiscard]] std::string make_header() const {
            std::ostringstream header_stream;
            if (config_.add_time) {
                header_stream << "DATE ";
            }
            if (config_.add_level) {
                fill_until_pos(header_stream, config_.custom_alignment.level_pos);
                header_stream << "LEVEL ";
            }
            if (config_.enable_service_name) {
                fill_until_pos(header_stream, config_.custom_alignment.service_pos);
                header_stream << "SERVICE ";
            }
            if (config_.add_thread) {
                fill_until_pos(header_stream, config_.custom_alignment.thread_pos);
                header_stream << "THREAD ID ";
            }
            if (config_.add_location) {
                fill_until_pos(header_stream, config_.custom_alignment.location_pos);
                header_stream << "LOCATION ";
            }

            if (config_.add_message) {
                fill_until_pos(header_stream, config_.custom_alignment.message_pos);
                header_stream << "MESSAGE ";
            }

            return header_stream.str();
        }
        std::string create_entry(const LogLevel level, const std::string_view message, const char* file, const int line,
            const char* function, const std::string_view service) const {
            std::ostringstream log_entry;
            if (config_.add_time) {
                log_entry << "[" << chrono::utilities::LocalClock::current_time_custom_fmt(config_.time_fmt) << "] ";
            }
            if (config_.add_level) {
                fill_until_pos(log_entry, config_.custom_alignment.level_pos);
                log_entry << "[" << to_string(level) << "] ";
            }
            if (config_.enable_service_name) {
                fill_until_pos(log_entry, config_.custom_alignment.service_pos);
                log_entry << "[" << service << "] ";
            }
            if (config_.add_thread) {
                fill_until_pos(log_entry, config_.custom_alignment.thread_pos);
                log_entry << "[Thread id: " << std::this_thread::get_id() << "] ";
            }
            if (config_.add_location) {
                fill_until_pos(log_entry, config_.custom_alignment.location_pos);
                log_entry << "[" << file << ":" << line;
                if (config_.add_pretty_function) {
                    log_entry << " " << function;
                }
                log_entry << "] ";
            }

            if (config_.add_message) {
                fill_until_pos(log_entry, config_.custom_alignment.message_pos);
                log_entry << message << "\n";
            }

            const std::string_view uncolored_entry = log_entry.view();
            switch (level) {
            case LogLevel::Debug:
                if (config_.enable_colors) {
                    return colors::make_white(uncolored_entry);
                }
                break;
            case LogLevel::Info:
                if (config_.enable_colors) {
                    return colors::make_green(uncolored_entry);
                }
                break;
            case LogLevel::Warning:
                if (config_.enable_colors) {
                    return colors::make_yellow(uncolored_entry);
                }
                break;
            case LogLevel::Error:
                if (config_.enable_colors) {
                    return colors::make_red(uncolored_entry);
                }
                break;
            case LogLevel::Fatal:
                if (config_.enable_colors) {
                    return colors::make_bold_red(uncolored_entry);
                }
                break;
            }
            return uncolored_entry.data();
        }
        explicit EntryProcessor(const EntryConfig& config) : config_(config) {}
        [[nodiscard]] EntryConfig& config() {
            return config_;
        }
        void set_config(const EntryConfig& config) {
            this->config_ = config;
        }

    private:
        EntryConfig config_;

        static void fill_until_pos(std::ostringstream& log, const std::size_t position) {
            if (const std::size_t padding = position - log.view().size(); log.view().size() <= position) {
                log << std::string(padding, filler);
            }
        }

        static constexpr char filler = ' ';
    };
} // namespace demiplane::scroll
