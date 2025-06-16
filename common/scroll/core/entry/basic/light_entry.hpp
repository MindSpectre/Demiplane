#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class LightEntry final : public detail::EntryBase<detail::MetaNone> // only level+msg
    {
        using Base = EntryBase;

    public:
        LightEntry(LogLevel lvl, const std::string_view& msg)
            : EntryBase<MetaNone>(lvl, msg, MetaNone{}) {}

        [[nodiscard]] std::string to_string() const {
            std::ostringstream formatter;
            formatter << "[" << scroll::to_string(level_) << "] " << message_ << "\n";
            return formatter.str();
        }
    };
} // namespace demiplane::scroll
