#pragma once

#include "../config_interface.hpp"


namespace demiplane::scroll {
    class FileTracerConfig final : public TracerConfigInterface {
    public:
        explicit FileTracerConfig(EntryConfig entry_cfg) : TracerConfigInterface(std::move(entry_cfg)) {}
        Json::Value dump() const override {
            Json::Value config = TracerConfigInterface::dump();
            return config;
        }
    };
}