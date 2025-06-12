#pragma once

#define LOG_DBG(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::DBG, message)
#define LOG_INF(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::INF, message)
#define LOG_WRN(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::WRN, message)
#define LOG_ERR(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::ERR, message)
#define LOG_FAT(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::FAT, message)
#define LOG_STREAM_DBG(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::DBG)
#define LOG_STREAM_INF(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::INF)
#define LOG_STREAM_WRN(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::WRN)
#define LOG_STREAM_ERR(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::ERR)
#define LOG_STREAM_FAT(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::FAT)


#ifdef ENABLE_LOGGING
#define LOG_ENTRY(logger_ptr, level, message) logger_ptr->log(level, message, __FILE__, __LINE__, __FUNCTION__)
#define LOG_STREAM_ENTRY(logger_ptr, level) demiplane::scroll::StreamLogEntry(logger_ptr, level, __FILE__, __LINE__, __FUNCTION__)
#else
#define LOG_ENTRY(logger_ptr, level, message) (void(0))
#define LOG_STREAM_ENTRY(logger_ptr, level) demiplane::scroll::DummyStreamLogEntry()

namespace demiplane::scroll {
    class DummyStreamLogEntry {
    public:
        template <typename T>
        DummyStreamLogEntry& operator<<(const T&) {
            return *this;
        }
    };
}
#endif
