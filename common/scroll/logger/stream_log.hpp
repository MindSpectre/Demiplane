// log_entry.hpp
#pragma once

#include <sstream>

#include "logger_interface.hpp"

#define LOG_STREAM_DBG(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::DBG)
#define LOG_STREAM_INF(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::INF)
#define LOG_STREAM_WRN(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::WRN)
#define LOG_STREAM_ERR(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::ERR)
#define LOG_STREAM_FAT(logger_ptr) LOG_STREAM_ENTRY(logger_ptr, demiplane::scroll::FAT)
#ifdef ENABLE_LOGGING
#define LOG_STREAM_ENTRY(logger_ptr, level) demiplane::scroll::StreamLogEntry(logger_ptr, level, __FILE__, __LINE__, __FUNCTION__)
#else
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
namespace demiplane::scroll {

    template<IsEntry EntryType>
    class StreamLogEntry {
    public:
        StreamLogEntry(Logger<EntryType>* logger_ptr, const LogLevel level, const char* file, const uint32_t line, const char* function)
            : logger_ptr_(logger_ptr), level_(level), file_(file), line_(line), function_(function) {}

        ~StreamLogEntry() {
            // In destructor, send the accumulated message to the logger
            if (logger_ptr_) {
                logger_ptr_->log(level_, stream_.str(), file_, line_, function_);
            }
        }

        // Stream operators for different types
        template<typename T>
        StreamLogEntry& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

    private:
        Logger<EntryType>* logger_ptr_;
        LogLevel level_;
        const char* file_;
        uint32_t line_;
        const char* function_;
        std::ostringstream stream_;
    };

} // namespace demiplane::scroll