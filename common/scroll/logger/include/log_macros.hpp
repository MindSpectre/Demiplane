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


#ifdef ENABLE_LOGGING
#define SCROLL_LOG_ENTRY(logger_ptr, level, message) logger_ptr->log(level, message, std::source_location::current())
#define SCROLL_LOG_STREAM_ENTRY(logger_ptr, level)   demiplane::scroll::StreamLogEntry(logger_ptr, level)
#else
#define LOG_ENTRY(logger_ptr, level, message) (void(0))
#define LOG_STREAM_ENTRY(logger_ptr, level)   demiplane::scroll::DummyStreamLogEntry()

namespace demiplane::scroll {
    class DummyStreamLogEntry {
    public:
        template <typename T>
        DummyStreamLogEntry& operator<<(const T&) {
            return *this;
        }
    };
} // namespace demiplane::scroll
#endif
