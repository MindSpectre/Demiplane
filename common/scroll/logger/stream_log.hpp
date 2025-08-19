#pragma once

#include <source_location>
#include <sstream>

#include "logger_interface.hpp"


namespace demiplane::scroll {
#ifndef ENABLE_LOGGING
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

    class StreamLogEntry {
    public:
        StreamLogEntry(Logger* logger_ptr,
                       const LogLevel level,
                       const std::source_location loc = std::source_location::current())
            : logger_ptr_(logger_ptr), level_(level), loc_(loc) {
        }

        ~StreamLogEntry() {
            // In destructor, send the accumulated message to the logger
            if (logger_ptr_) {
                logger_ptr_->log(level_, stream_.str(), loc_);
            }
        }

        // Stream operators for different types
        template <typename T>
        StreamLogEntry& operator<<(const T& value) {
            stream_ << value;
            return *this;
        }

    private:
        Logger* logger_ptr_;
        LogLevel level_;
        std::source_location loc_;
        std::ostringstream stream_;
    };
} // namespace demiplane::scroll
