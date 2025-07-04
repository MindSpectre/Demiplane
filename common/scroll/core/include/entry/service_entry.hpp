#pragma once

#include <sstream>
#include <string>

#include "../entry_interface.hpp"
#include <concepts.hpp>
#include <clock.hpp>
namespace demiplane::scroll {
    template <gears::HasStaticNameM Service>
    class ServiceEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread, detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;
        [[nodiscard]] std::string to_string() const override {
            std::ostringstream os;
            os << chrono::UTCClock::format_time(time_point, chrono::clock_formats::eu_dmy_hms) << " ["
               << log_level_to_string(level_) << "] "
               << "[" << Service::name << "] "
               << "[" << loc.file_name() << ':' << loc.line() << " " << loc.function_name() << "] "
               << "[tid " << tid << ", pid " << pid << "] " << message_ << '\n';
            return os.str();
        }
        static bool comp(const ServiceEntry& lhs, const ServiceEntry& rhs) {
            if (lhs.time_point == rhs.time_point) {
                return lhs.level() < rhs.level();
            }
            return lhs.time_point < rhs.time_point;
        }
    };
    template <class Service>
    struct detail::entry_traits<ServiceEntry<Service>> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess>;
    };
} // namespace demiplane::scroll
