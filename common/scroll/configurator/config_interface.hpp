#pragma once

#include <utility>

#include "entry_config.hpp"
namespace demiplane::scroll {
    class ScrollConfigInterface {
    public:
        virtual ~ScrollConfigInterface() = default;
        explicit ScrollConfigInterface(const EntryConfig& entry_cfg) : entry_cfg_(entry_cfg) {}
        ScrollConfigInterface(const EntryConfig& entry_cfg, std::string service_name) : service_name_(std::move(service_name)), entry_cfg_(entry_cfg) {}
        [[nodiscard]] const EntryConfig& get_entry_cfg() const {
            return entry_cfg_;
        }
        void set_entry_cfg(const EntryConfig &entry_cfg) {
            this->entry_cfg_ = entry_cfg;
        }
        [[nodiscard]] virtual Json::Value dump() const {
            return entry_cfg_.dump_config();
        }

    protected:
        std::string service_name_ = "N/A";
        EntryConfig entry_cfg_;
    };

    class TracerConfigInterface : public ScrollConfigInterface {
    public:
        explicit TracerConfigInterface(const EntryConfig &entry_cfg) : ScrollConfigInterface(entry_cfg) {}
        [[nodiscard]] LogLevel get_threshold() const {
            return threshold;
        }
        void set_threshold(const LogLevel threshold) {
            this->threshold = threshold;
        }

    protected:
        LogLevel threshold{LogLevel::Debug};
    };

    class LoggerConfigInterface : public ScrollConfigInterface {
    public:
        explicit LoggerConfigInterface(const EntryConfig &entry_cfg) : ScrollConfigInterface(entry_cfg) {}

    protected:
        uint32_t max_records_{1 << 20};
    };
} // namespace demiplane::scroll
