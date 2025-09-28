#pragma once

#define SET_COMMON_LOGGER()                                                                                            \
    demiplane::scroll::FileLoggerConfig cfg;                                                                           \
    cfg.file                 = "query_test.log";                                                                       \
    cfg.add_time_to_filename = false;                                                                                  \
    demiplane::nexus::instance().register_singleton<demiplane::scroll::Logger>([] {                                    \
        return std::make_shared<demiplane::scroll::FileLogger<demiplane::scroll::DetailedEntry>>(                      \
            *demiplane::nexus::instance().get<demiplane::scroll::FileLoggerConfig>());                                 \
    });                                                                                                                \
    demiplane::nexus::instance().register_singleton<demiplane::scroll::FileLoggerConfig>(std::move(cfg));              \
    set_logger(demiplane::nexus::instance().get<demiplane::scroll::Logger>())
