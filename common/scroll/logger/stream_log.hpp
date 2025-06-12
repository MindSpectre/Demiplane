// log_entry.hpp
#pragma once

#include <sstream>

#include "logger_interface.hpp"


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