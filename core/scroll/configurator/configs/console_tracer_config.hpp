#pragma once

#include "../config_interface.hpp"

namespace demiplane::scroll {

    class ConsoleTracerConfig final : public TracerConfigInterface {
    public:
        explicit ConsoleTracerConfig(const EntryConfig& entry_cfg) : TracerConfigInterface(entry_cfg) {}
        [[nodiscard]] Json::Value dump() const override {
            Json::Value config = TracerConfigInterface::dump();
            return config;
        }
    };
}