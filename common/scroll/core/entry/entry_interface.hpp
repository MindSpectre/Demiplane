#pragma once

#include "../log_level.hpp"

namespace demiplane::scroll {
    class Entry {
    public:
        virtual ~Entry() = default;
        Entry(const LogLevel level, const std::string_view& message, const std::string_view& file, const uint32_t line,
            const std::string_view& function)
            : level_(level), message_(message), file_(file), line_(line), function_(function) {}
        [[nodiscard]] virtual std::string to_string() const = 0;
        [[nodiscard]] LogLevel level() const {
            return level_;
        }

    protected:
        LogLevel level_{LogLevel::Debug};
        std::string_view message_;
        std::string_view file_;
        uint32_t line_;
        std::string_view function_;
    };
    template <typename AnyEntry>
    concept IsEntry = std::is_base_of_v<Entry, AnyEntry>;

} // namespace demiplane::scroll
