#pragma once

#include <memory>

#include "../configs/console_tracer_config.hpp"
#include "../configs/file_tracer_config.hpp"

// TODO: Review possible useless
namespace demiplane::scroll {

    class ScrollConfigFactory final {
    public:
        static ConsoleTracerConfig create_default_console_tracer_config() {
            return ConsoleTracerConfig{{}};
        }
        static FileTracerConfig create_default_file_tracer_config() {
            return FileTracerConfig{{}};
        }
        static std::unique_ptr<ScrollConfigInterface> create_file_tracer_config(const EntryConfig& entry_config) {
            return std::make_unique<FileTracerConfig>(entry_config);
        }
    };
} // namespace demiplane::scroll
