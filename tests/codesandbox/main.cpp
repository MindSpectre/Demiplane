#define ENABLE_LOGGING
#include <demiplane/scroll>

int main() {
    demiplane::scroll::ConsoleLogger<demiplane::scroll::DetailedEntry> logger;
    // logger.log(LIGHT_LOG_ENTRY(demiplane::scroll::DBG, "Hello, world!"));
    // SUMMON_STREAM(LOGGER) <<
    LOG_STREAM_ERR(&logger) << "Hello, world!";
    return 0;
}
