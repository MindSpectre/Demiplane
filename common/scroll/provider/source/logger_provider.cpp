#include "logger_provider.hpp"

#include <demiplane/nexus>

void demiplane::scroll::ComponentLoggerManager::initialize() {
    try {
        if (const std::shared_ptr<Logger> nexus_logger = nexus::instance().get<Logger>()) {
            logger_ = nexus_logger;
            return;
        }
    } catch (...) {
        // TODO: process error
        //  Nexus not available or logger not registered
    }
    // Fallback: create our own logger
    logger_ = std::make_shared<Logger>();

    // Default configuration: Console sink with colored output
    logger_->add_sink(std::make_unique<ConsoleSink<DetailedEntry>>(
        ConsoleSinkConfig{}.threshold(LogLevel::Debug).enable_colors(true).flush_each_entry(false)));
}
