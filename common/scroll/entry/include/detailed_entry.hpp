#pragma once

#include <string>

#include "../entry_interface.hpp"

namespace demiplane::scroll {
    class DetailedEntry final
        : public detail::EntryBase<detail::MetaTimePoint, detail::MetaSource, detail::MetaThread,
                                   detail::MetaProcess> {
    public:
        using EntryBase::EntryBase;

        [[nodiscard]] std::string to_string() const override;

        static bool comp(const DetailedEntry& lhs, const DetailedEntry& rhs) {
            // Optimized comparison using single 64-bit compare when possible
            if (lhs.time_point != rhs.time_point) {
                return lhs.time_point < rhs.time_point;
            }
            return lhs.level() < rhs.level();
        }
    };

    // Traits for the new fast entry
    template <>
    struct detail::entry_traits<DetailedEntry> {
        using wants = gears::type_list<MetaTimePoint, MetaSource, MetaThread, MetaProcess>;
    };
}
