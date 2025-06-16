#pragma once

#include "clock.hpp"
#include "custom_entry.hpp"
#include "detailed_entry.hpp"
#include "light_entry.hpp"
#include "service_entry.hpp"
namespace demiplane::scroll {

    template <class EntryT>
    EntryT make_entry(
        LogLevel lvl, std::string_view msg, const std::source_location loc = std::source_location::current()) {
        const detail::MetaSource location{loc};
        const detail::MetaThread tid{std::hash<std::thread::id>{}(std::this_thread::get_id())};
        const detail::MetaProcess pid{getpid()};
        const detail::MetaTimePoint time_point{chrono::Clock::current_time()};
        if constexpr (std::is_same_v<EntryT, LightEntry>) {
            return LightEntry{lvl, msg};
        } else if constexpr (std::is_same_v<EntryT, DetailedEntry>) {
            return DetailedEntry{lvl, msg, time_point, location, tid, pid};
        } else if constexpr (gears::is_specialisation_of_v<ServiceEntry, EntryT>) {
            return EntryT{lvl, msg, time_point, location, tid, pid};
        } else if constexpr (std::is_same_v<EntryT, CustomEntry>) {
            const CustomEntryConfig cfg;
            return CustomEntry{lvl, msg, time_point, location, tid, cfg};
        } else {
            gears::unreachable<EntryT>();
            return {};
        }
    }

} // namespace demiplane::scroll
