#pragma once

#define SCROLL_LOG_DIRECT_DBG(logger_ptr, message) SCROLL_LOG_ENTRY(logger_ptr, demiplane::scroll::DBG, message)
#define SCROLL_LOG_DIRECT_INF(logger_ptr, message) SCROLL_LOG_ENTRY(logger_ptr, demiplane::scroll::INF, message)
#define SCROLL_LOG_DIRECT_WRN(logger_ptr, message) SCROLL_LOG_ENTRY(logger_ptr, demiplane::scroll::WRN, message)
#define SCROLL_LOG_DIRECT_ERR(logger_ptr, message) SCROLL_LOG_ENTRY(logger_ptr, demiplane::scroll::ERR, message)
#define SCROLL_LOG_DIRECT_FAT(logger_ptr, message) SCROLL_LOG_ENTRY(logger_ptr, demiplane::scroll::FAT, message)
#define SCROLL_LOG_DIRECT_STREAM_DBG(logger_ptr)   SCROLL_LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::DBG)
#define SCROLL_LOG_DIRECT_STREAM_INF(logger_ptr)   SCROLL_LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::INF)
#define SCROLL_LOG_DIRECT_STREAM_WRN(logger_ptr)   SCROLL_LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::WRN)
#define SCROLL_LOG_DIRECT_STREAM_ERR(logger_ptr)   SCROLL_LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::ERR)
#define SCROLL_LOG_DIRECT_STREAM_FAT(logger_ptr)   SCROLL_LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::FAT)


#ifdef DMP_ENABLE_LOGGING
#define SCROLL_LOG_ENTRY(logger_ptr, level, message) logger_ptr->log(level, message, std::source_location::current())
#define SCROLL_LOG_STREAM_ENTRY(logger_ptr, level)   demiplane::scroll::StreamLogEntry(logger_ptr, level)
#else
#define SCROLL_LOG_ENTRY(logger_ptr, level, message) (void(0))
#define SCROLL_LOG_STREAM_ENTRY(logger_ptr, level)   demiplane::scroll::DummyStreamLogEntry()
#endif


#ifdef DMP_COMPONENT_LOGGING
// Direct message logging
    #define COMPONENT_LOG(level, message) SCROLL_LOG_DIRECT_##level(::demiplane::scroll::ComponentLoggerManager::get(), message)
// Stream-based logging
    #define COMPONENT_LOG_DBG() SCROLL_LOG_DIRECT_STREAM_DBG(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_INF()  SCROLL_LOG_DIRECT_STREAM_INF(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_WRN() SCROLL_LOG_DIRECT_STREAM_WRN(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_ERR() SCROLL_LOG_DIRECT_STREAM_ERR(::demiplane::scroll::ComponentLoggerManager::get())
    #define COMPONENT_LOG_FAT() SCROLL_LOG_DIRECT_STREAM_FAT(::demiplane::scroll::ComponentLoggerManager::get())
#else
// When component logging is disabled, use scroll's dummy implementations
    #define COMPONENT_LOG(level, message) ((void)0)
    #define COMPONENT_LOG_DBG() demiplane::scroll::DummyStreamLogEntry()
    #define COMPONENT_LOG_INF() demiplane::scroll::DummyStreamLogEntry()
    #define COMPONENT_LOG_WRN() demiplane::scroll::DummyStreamLogEntry()
    #define COMPONENT_LOG_ERR() demiplane::scroll::DummyStreamLogEntry()
    #define COMPONENT_LOG_FAT() demiplane::scroll::DummyStreamLogEntry()
#endif
