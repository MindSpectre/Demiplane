#pragma once

#include <sstream>
#include <string>

#include "entry_interface.hpp"

namespace demiplane::scroll {
    class LightEntry final : public detail::EntryBase<detail::MetaNone>  // only level+msg
    {
    public:
        using EntryBase::EntryBase;
        LightEntry(const LogLevel lvl, const std::string_view msg)
            : EntryBase(lvl, msg, MetaNone{}) {
        }

        [[nodiscard]] const std::string& to_string() const override {
            std::string& buf = get_tl_buffer();
            buf.clear();
            buf.append(log_level_to_string(level_));
            buf.push_back(' ');
            buf.append(message_);
            buf.push_back('\n');
            return buf;
        }
        static bool comp(const LightEntry& lhs, const LightEntry& rhs) {
            gears::unused_value(lhs, rhs);
            static_assert(true, "Can not be sorted");
            throw std::logic_error("Can not be sorted");
        }
    };
    template <>
    struct detail::entry_traits<LightEntry> {
        using wants = gears::type_list<>;
    };
}  // namespace demiplane::scroll
