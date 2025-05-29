#pragma once

#include <string_view>
#include "entry/entry_interface.hpp"
#define LOG_DBG(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::DBG, message)
#define LOG_INF(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::INF, message)
#define LOG_WRN(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::WRN, message)
#define LOG_ERR(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::ERR, message)
#define LOG_FAT(logger_ptr, message) LOG_ENTRY(logger_ptr, demiplane::scroll::FAT, message)

#ifdef ENABLE_LOGGING
#define LOG_ENTRY(logger_ptr, level, message) (logger_ptr)->(level, __FILE__, __LINE__, __FUNCTION__)
#else
#define LOG_ENTRY(logger_ptr, level, message) (void(0))
#endif

namespace demiplane::scroll {

    template<IsEntry EntryType>
    class Logger {
    public:
        virtual ~Logger() = default;

        virtual void log(LogLevel level, std::string_view message, const char* file, const int line,
                         const char* function) = 0;

        virtual void log(EntryType entry) = 0;

        virtual void set_threshold(const LogLevel level) {
            threshold_ = level;
        }

        [[nodiscard]] LogLevel get_threshold() const {
            return threshold_;
        }

    protected:
        LogLevel threshold_{LogLevel::Debug};
    };

} // namespace demiplane::scroll