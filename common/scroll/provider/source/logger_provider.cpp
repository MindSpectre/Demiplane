#include "logger_provider.hpp"

#include <demiplane/nexus>

void demiplane::scroll::ComponentLoggerManager::initialize() {
    // Try to get from Nexus first
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
    logger_ = std::make_shared<ConsoleLogger<LightEntry>>(ConsoleLoggerConfig{});
}
