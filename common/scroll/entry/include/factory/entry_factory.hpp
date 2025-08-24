#pragma once

#include "custom_entry.hpp"
#include "detailed_entry.hpp"
#include "light_entry.hpp"
#include "service_entry.hpp"

namespace demiplane::scroll {
    template <class EntryT, class... Extra>
    EntryT make_entry(LogLevel lvl,
                      const std::string_view msg,
                      const detail::MetaSource& loc,
                      Extra&&... extra) {
        // Use cached thread-local values
        auto available = std::tuple{
            detail::MetaTimePoint{chrono::Clock::now()},
            loc,
            // Already lightweight
            detail::MetaThread{},
            // Uses cached values
            detail::MetaProcess{},
            // Uses cached values
            std::forward<Extra>(extra)...
        };

        using want_types = detail::entry_traits<EntryT>::wants;
        auto args        = gears::make_arg_tuple<want_types, decltype(available)>::from(std::move(available));

        return std::apply(
            [&]<typename... Tailored>(Tailored&&... tail) {
                return EntryT{lvl, msg, std::forward<Tailored>(tail)...};
            },
            args);
    }
} // namespace demiplane::scroll
