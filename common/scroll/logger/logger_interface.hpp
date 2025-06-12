#pragma once

#include <string_view>

#include "entry/entry_interface.hpp"


namespace demiplane::scroll {

    template <IsEntry EntryType>
    class Logger {
    public:
        virtual ~Logger() = default;

        explicit Logger(const LogLevel level) : threshold_{level} {}

        Logger() : threshold_{LogLevel::Debug} {}

        virtual void log(
            LogLevel level, std::string_view message, const char* file, uint32_t line, const char* function) = 0;

        virtual void log(const EntryType &entry) = 0;

        virtual void set_threshold(const LogLevel level) {
            threshold_ = level;
        }

        [[nodiscard]] LogLevel get_threshold() const {
            return threshold_;
        }

    protected:
        LogLevel threshold_;
    };

} // namespace demiplane::scroll
