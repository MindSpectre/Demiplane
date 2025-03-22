#pragma once
#include <json/json.h>
#include <string>

#include "chrono_utils.hpp"
#include "traits_classes.hpp"
namespace demiplane::scroll {
    enum class LogLevel {
        Debug = 0,
        Info,
        Warning,
        Error,
        Fatal
        // Extend with additional levels if needed.
    };

    constexpr const char* to_string(const LogLevel level) {
        switch (level) {
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Fatal:
            return "FATAL";
        }
        return "UNKNOWN";
    }
    class EntryConfig : Immovable {
    public:
        bool add_time            = true;
        bool add_level           = true;
        bool add_location        = true;
        bool add_pretty_function = false;
        bool add_thread          = true;
        bool add_message         = true;
        bool enable_header       = true;
        bool enable_colors       = true;
        bool enable_service_name = true;

        std::string time_fmt = "%d-%m-%Y %X";
        struct Alignment {
            std::size_t time_pos     = 0;
            std::size_t level_pos    = 30;
            std::size_t service_pos  = 40;
            std::size_t thread_pos   = 55;
            std::size_t location_pos = 95;
            std::size_t message_pos  = 185;
            [[nodiscard]] bool ok() const {
                return message_pos > location_pos && location_pos > thread_pos && thread_pos > service_pos
                    && service_pos > level_pos && level_pos > time_pos;
            }
            void disable_alignment() {
                time_pos     = 0;
                location_pos = 0;
                thread_pos   = 0;
                service_pos  = 0;
                level_pos    = 0;
                message_pos  = 0;
            }
        };
        Alignment custom_alignment;
        bool load_config(const std::string& config_file_path);
        [[nodiscard]] Json::Value dump_config() const {
            void(this);
            return {};
        }
    };


} // namespace demiplane::scroll
