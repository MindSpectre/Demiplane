#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include "custom_entry.hpp"
#include "chrono_utils.hpp"
#include "colors.hpp"
namespace {
    void fill_until_pos(std::ostringstream& log, const std::size_t position) {
        if (const std::size_t padding = position - log.view().size(); log.view().size() <= position) {
            log << std::string(padding, ' ');
        }
    }
}
namespace demiplane::scroll {
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
        if (root.isMember("alignment")) {
            Alignment proposed_alignment;
            const Json::Value& alignment = root["alignment"];
            if (alignment.isArray()) {
                proposed_alignment.time_pos     = alignment[0].asInt();
                proposed_alignment.level_pos    = alignment[1].asInt();
                proposed_alignment.thread_pos   = alignment[2].asInt();
                proposed_alignment.location_pos = alignment[3].asInt();
                proposed_alignment.message_pos  = alignment[4].asInt();
            }
            if (alignment.isObject()) {
                if (alignment.isMember("time_pos")) {
                    proposed_alignment.time_pos = alignment["time_pos"].asInt();
                }
                if (alignment.isMember("level_pos")) {
                    proposed_alignment.level_pos = alignment["level_pos"].asInt();
                }
                if (alignment.isMember("thread_pos")) {
                    proposed_alignment.time_pos = alignment["thread_pos"].asInt();
                }
                if (alignment.isMember("location_pos")) {
                    proposed_alignment.time_pos = alignment["location_pos"].asInt();
                }
                if (alignment.isMember("message_pos")) {
                    proposed_alignment.time_pos = alignment["message_pos"].asInt();
                }
            }
        }
        return true;
    }

    std::string CustomEntryConfig::make_header() const {
        std::ostringstream header_stream;
        if (add_time) {
            header_stream << "DATE ";
        }
        if (add_level) {
            fill_until_pos(header_stream, custom_alignment.level_pos);
            header_stream << "LEVEL ";
        }
        if (enable_service_name) {
            fill_until_pos(header_stream, custom_alignment.service_pos);
            header_stream << "SERVICE ";
        }
        if (add_thread) {
            fill_until_pos(header_stream, custom_alignment.thread_pos);
            header_stream << "THREAD ID ";
        }
        if (add_location) {
            fill_until_pos(header_stream, custom_alignment.location_pos);
            header_stream << "LOCATION ";
        }

        if (add_message) {
            fill_until_pos(header_stream, custom_alignment.message_pos);
            header_stream << "MESSAGE ";
        }

        return header_stream.str();
    }

    std::string CustomEntry::to_string() const {
        std::ostringstream log_entry;
        if (config_.add_time) {
            log_entry << "[" << chrono::utilities::LocalClock::current_time_custom_fmt(config_.time_fmt) << "] ";
        }
        if (config_.add_level) {
            fill_until_pos(log_entry, config_.custom_alignment.level_pos);
            log_entry << "[" << scroll::to_string(level_) << "] ";
        }
        // if (config_.enable_service_name) {
        //     fill_until_pos(log_entry, config_.custom_alignment.service_pos);
        //     log_entry << "[" << service_ << "] ";
        // }
        if (config_.add_thread) {
            fill_until_pos(log_entry, config_.custom_alignment.thread_pos);
            log_entry << "[Thread id: " << std::this_thread::get_id() << "] ";
        }
        if (config_.add_location) {
            fill_until_pos(log_entry, config_.custom_alignment.location_pos);
            log_entry << "[" << file_ << ":" << line_;
            if (config_.add_pretty_function) {
                log_entry << " " << function_;
            }
            log_entry << "] ";
        }

        if (config_.add_message) {
            fill_until_pos(log_entry, config_.custom_alignment.message_pos);
            log_entry << message_ << "\n";
        }

        const std::string_view uncolored_entry = log_entry.view();
        switch (level_) {
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

} // namespace demiplane::tracing


