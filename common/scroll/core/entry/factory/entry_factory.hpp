#pragma once

#include "../custom/include/custom_entry.hpp"
#include "clock.hpp"
#include "detailed_entry.hpp"
#include "light_entry.hpp"
#include "service_entry.hpp"
namespace demiplane::scroll {

    template <class EntryT, class... Extra>
    EntryT make_entry(LogLevel lvl, const std::string_view msg, const std::source_location loc, Extra&&... extra) {
        // 1) collect *runtime* objects only once
        auto available = std::tuple{
            detail::MetaTimePoint{chrono::Clock::current_time()}, detail::MetaSource{loc},
            detail::MetaThread{std::hash<std::thread::id>{}(std::this_thread::get_id())}, detail::MetaProcess{getpid()},
            std::forward<Extra>(extra)... // e.g. cfg
        };

        // 2) build the tuple the Entry wants
        using want_types = typename detail::entry_traits<EntryT>::wants;
        auto args        = gears::make_arg_tuple<want_types, decltype(available)>::from(std::move(available));

        // 3) prepend lvl + msg and call the constructor
        return std::apply(
            [&]<typename... Tailored>(Tailored&&... tail) { return EntryT{lvl, msg, std::forward<Tailored>(tail)...}; },
            args);
    }

} // namespace demiplane::scroll
