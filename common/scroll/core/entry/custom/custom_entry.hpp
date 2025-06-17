#pragma once
#include <demiplane/gears>
#include <utility>
#include <json/json.h>

#include "entry/entry_interface.hpp"

namespace demiplane::scroll {


    /**
     * @class CustomEntryConfig
     * @brief This class provides configuration settings for custom log entries.
     * It allows for fine-grained control over the format and content of log messages.
     *
     * CustomEntryConfig provides various options to enable or disable specific elements in the log entry,
     * such as timestamps, log levels, source locations, thread IDs, and messages. Additionally,
     * it supports alignment settings, color formatting, and customization of the time format string.
     *
     * The configuration is applied to generate consistent log entries and headers based on the user's preferences.
     *
     * Inherits from the `gears::Immovable` class to restrict its copy or move semantics.
     */
    class CustomEntryConfig {
    public:
        bool add_time            = true;
        bool add_level           = true;
        bool add_location        = true;
        bool add_pretty_function = false;
        bool add_thread          = false;
        bool add_message         = true;
        bool enable_header       = false;
        bool enable_colors       = true;
        bool enable_service_name = true;
        const char* time_fmt     = "%d-%m-%Y %X";

        struct Alignment {
            std::size_t time_pos     = 0;
            std::size_t level_pos    = 0;
            std::size_t service_pos  = 0;
            std::size_t thread_pos   = 0;
            std::size_t location_pos = 0;
            std::size_t message_pos  = 0;

            void disable_alignment() {
                time_pos     = 0;
                location_pos = 0;
                thread_pos   = 0;
                service_pos  = 0;
                level_pos    = 0;
                message_pos  = 0;
            }
            void set_default() {
                time_pos     = 0;
                thread_pos   = 70;
                service_pos  = 40;
                level_pos    = 30;
                location_pos = 100;
                message_pos  = 200;
            }
        };
        Alignment custom_alignment;
        bool load_config(const std::string& config_file_path);
        [[nodiscard]] std::string make_header() const;
        [[nodiscard]] Json::Value dump_config() const {
            gears::force_non_static(this);
            return {};
        }
    };
    class CustomEntry final : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread> {
    public:
        CustomEntry(const LogLevel lvl, const std::string_view& msg, const MetaTimePoint& meta_time_point,
            const MetaSource& meta_source, const MetaThread& meta_thread, std::shared_ptr<CustomEntryConfig> config)
            : EntryBase(lvl, msg, meta_time_point, meta_source, meta_thread), config_(std::move(config)) {}

        [[nodiscard]] std::string to_string() const;

    private:
        std::shared_ptr<CustomEntryConfig> config_;
    };
    template <>
    struct detail::entry_traits<CustomEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, std::shared_ptr<CustomEntryConfig>>;
    };
} // namespace demiplane::scroll
