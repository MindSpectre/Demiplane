#include "custom_entry.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <demiplane/chrono>

#include <colors.hpp>

// namespace
namespace demiplane::scroll {
    /**
     * Loads the configuration settings from a specified JSON file.
     *
     * This method reads a JSON configuration file and updates internal
     * configuration parameters based on the parsed values. It supports both
     * Boolean and string representations for certain fields and applies
     * custom logic to interpret them.
     *
     * @param config_file_path The path to the configuration file to be loaded.
     * @return `true` if the configuration file is successfully loaded and parsed,
     *         `false` otherwise.
     */
    inline bool CustomEntryConfig::load_config(const std::string& config_file_path) {
        std::ifstream config_file(config_file_path);
        if (!config_file.is_open()) {
            return false;
        }
        Json::Value root;
        std::string errs;
        if (Json::CharReaderBuilder reader; !parseFromStream(reader, config_file, &root, &errs)) {
            std::cerr << "Error parsing configuration: " << errs << std::endl;
            return false;
        }
        auto deduce_cond = [&](const Json::Value& jsons, const std::string& field) {
            if (jsons[field].isBool()) {
                return jsons[field].asBool();
            }
            if (jsons[field].isString()) {
                std::string status = jsons[field].asString();
                std::transform(status.begin(), status.end(), status.begin(), tolower);
                if (status == "true" || status == "enabled" || status == "enable") {
                    return true;
                }
                if (status == "false" || status == "disabled" || status == "disable") {
                    return false;
                }
            }
            return false;
        };
        if (root.isMember("time")) {
            add_time = deduce_cond(root, "time");
        }
        // if (root.isMember("log_level") && root["log_level"].isString()) {
        //     std::string level = root["log_level"].asString();
        //     std::transform(level.begin(), level.end(), level.begin(), tolower);
        //     if (level == "debug") {
        //         threshold = LogLevel::Debug;
        //     } else if (level == "info") {
        //         threshold = LogLevel::Info;
        //     } else if (level == "warning") {
        //         threshold = LogLevel::Warning;
        //     } else if (level == "error") {
        //         threshold = LogLevel::Error;
        //     } else if (level == "fatal") {
        //         threshold = LogLevel::Fatal;
        //     }
        // }

        if (root.isMember("location")) {
            add_location = deduce_cond(root, "location");
        }
        if (root.isMember("thread")) {
            add_thread = deduce_cond(root, "thread");
        }
        if (root.isMember("message")) {
            add_message = deduce_cond(root, "message");
        }
        if (root.isMember("header")) {
            enable_header = deduce_cond(root, "header");
        }
        if (root.isMember("colors")) {
            enable_colors = deduce_cond(root, "colors");
        }

        return true;
    }

    std::string CustomEntryConfig::make_header() const {
        std::ostringstream header_stream;
        if (add_time) {
            header_stream << "DATE ";
        }
        if (add_level) {

            header_stream << "LEVEL ";
        }
        if (add_thread) {

            header_stream << "THREAD ID ";
        }
        if (add_location) {

            header_stream << "LOCATION ";
        }

        if (add_message) {

            header_stream << "MESSAGE ";
        }

        return header_stream.str();
    }

    std::string CustomEntry::to_string() const {
        std::ostringstream log_entry;
        if (config_->add_time) {
            log_entry << chrono::UTCClock::format_time(time_point, config_->time_fmt) << " ";
        }
        if (config_->add_level) {
            log_entry << log_level_to_string(level_) << " ";
        }
        if (config_->add_thread) {

            log_entry << "[Thread id: " << std::this_thread::get_id() << "] ";
        }
        if (config_->add_location) {
            log_entry << "[" << location.file_name() << ":" << location.line();
            if (config_->add_pretty_function) {
                log_entry << " " << location.function_name();
            }
            log_entry << "] ";
        }

        if (config_->add_message) {

            log_entry << message_ << "\n";
        }

        std::string uncolored_entry = log_entry.str();
        switch (level_) {
            case LogLevel::Trace:
            case LogLevel::Debug:
            if (config_->enable_colors) {
                return colors::make_white(uncolored_entry);
            }
            break;
        case LogLevel::Info:
            if (config_->enable_colors) {
                return colors::make_green(uncolored_entry);
            }
            break;
        case LogLevel::Warning:
            if (config_->enable_colors) {
                return colors::make_yellow(uncolored_entry);
            }
            break;
        case LogLevel::Error:
            if (config_->enable_colors) {
                return colors::make_red(uncolored_entry);
            }
            break;
        case LogLevel::Fatal:
            if (config_->enable_colors) {
                return colors::make_bold_red(uncolored_entry);
            }
            break;
        }
        return uncolored_entry;
    }

} // namespace demiplane::scroll
