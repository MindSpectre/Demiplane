#pragma once
#include <utility>
#include <json/json.h>

#include "../entry_interface.hpp"

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
        bool                      add_time            = true;
        bool                      add_level           = true;
        bool                      add_location        = true;
        bool                      add_pretty_function = false;
        bool                      add_thread          = false;
        bool                      add_message         = true;
        bool                      enable_header       = false;
        bool                      enable_colors       = true;
        const char*               time_fmt            = "%d-%m-%Y %X";
        bool                      load_config(const std::string& config_file_path);
        [[nodiscard]] std::string make_header() const;

        [[nodiscard]] Json::Value dump_config() const {
            gears::enforce_non_static(this);
            return {};
        }
    };

    class CustomEntry final : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread> {
    public:
        CustomEntry(const LogLevel                     lvl,
                    const std::string_view&            msg,
                    const MetaTimePoint&               meta_time_point,
                    const MetaSource&                  meta_source,
                    const MetaThread&                  meta_thread,
                    std::shared_ptr<CustomEntryConfig> config)
            : EntryBase(lvl, msg, meta_time_point, meta_source, meta_thread),
              config_(std::move(config)) {}

        [[nodiscard]] std::string to_string() const override;

        static bool comp(const CustomEntry& lhs, const CustomEntry& rhs) {
            if (lhs.time_point == rhs.time_point) {
                return lhs.level() < rhs.level();
            }
            return lhs.time_point < rhs.time_point;
        }

    private:
        std::shared_ptr<CustomEntryConfig> config_;
    };

    template <>
    struct detail::entry_traits<CustomEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, std::shared_ptr<CustomEntryConfig>>;
    };
} // namespace demiplane::scroll
