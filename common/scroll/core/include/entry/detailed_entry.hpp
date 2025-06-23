#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"
#include <clock.hpp>
#include <templates.hpp>

namespace demiplane::scroll {
    class DetailedEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;

        [[nodiscard]] std::string to_string() const {
            std::ostringstream os;
            os << chrono::UTCClock::format_time(time_point, chrono::clock_formats::ymd_hms) << " ["
               << log_level_to_string(level_) << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
    };
    template <>
    struct detail::entry_traits<DetailedEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess>;
    };
} // namespace demiplane::scroll
