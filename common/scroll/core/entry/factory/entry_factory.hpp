#pragma once

#include "detailed_entry.hpp"
#include "gears_utils.hpp"
#include "light_entry.hpp"
#include "service_entry.hpp"

namespace demiplane::scroll {

    template <class EntryT>
    EntryT make_entry(LogLevel lvl, std::string_view msg, std::source_location loc = std::source_location::current()) {
        // step 1: build tuple conditionally
        detail::MetaSource location{loc};
        detail::MetaThread tid{std::hash<std::thread::id>{}(std::this_thread::get_id())};
        detail::MetaProcess pid{getpid()};
        if constexpr (std::is_same_v<EntryT, LightEntry>) {
            return LightEntry{lvl, msg};
        } else if constexpr (std::is_same_v<EntryT, DetailedEntry>) {
             return DetailedEntry{lvl, msg, location, tid, pid};
        } else if constexpr(gears::is_specialisation_of_v<ServiceEntry, EntryT>) {
            return EntryT{lvl, msg, location, tid, pid};
        } else {
            gears::unreachable<EntryT>();
            return {};
        }

    }
}