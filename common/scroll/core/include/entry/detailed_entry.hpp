#pragma once

#include <sstream>
#include <string>

#include <clock.hpp>
#include <gears_templates.hpp>
#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class DetailedEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;

        [[nodiscard]] std::string to_string() const override {
            std::ostringstream os;
            os << chrono::UTCClock::format_time_iso_ms(time_point)<< " ["
               << log_level_to_string(level_) << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
        static bool comp(const DetailedEntry& lhs, const DetailedEntry& rhs) {
            if (lhs.time_point == rhs.time_point) {
                return lhs.level() < rhs.level();
            }
            return lhs.time_point < rhs.time_point;
        }
    };
    template <>
    struct detail::entry_traits<DetailedEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess>;
    };
} // namespace demiplane::scroll
