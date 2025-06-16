#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"
#include "clock.hpp"
namespace demiplane::scroll {
    template <class Service>
    class ServiceEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;
        [[nodiscard]] std::string to_string() const {
            std::ostringstream os;
            os << chrono::UTCClock::format_time(time_point, chrono::clock_formats::ymd_hms) << " ["
               << scroll::to_string(level_) << "] "
               << "[" << Service::name() << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
    };
} // namespace demiplane::scroll
