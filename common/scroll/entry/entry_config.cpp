#include "entry_config.hpp"

#include <fstream>
#include <iostream>
namespace demiplane::scroll {

    inline bool EntryConfig::load_config(const std::string& config_file_path) {
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
                if (proposed_alignment.ok()) {
                    custom_alignment = proposed_alignment;
                } else {
                    std::cerr << "Proposed alignment is not valid. Default values has been set" << std::endl;
                }
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
                if (proposed_alignment.ok()) {
                    custom_alignment = proposed_alignment;
                } else {
                    std::cerr << "Proposed alignment is not valid. Default values has been set" << std::endl;
                }
            }
        }
        return true;
    }

} // namespace demiplane::tracing


