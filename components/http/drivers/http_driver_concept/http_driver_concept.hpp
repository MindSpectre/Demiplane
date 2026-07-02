#pragma once

#include <concepts>
#include <span>
#include <string_view>

#include <http_enums.hpp>

namespace demiplane::http {

    /**
     * @brief What every protocol driver advertises statically (spec §6.2).
     *
     * serve(Connection&, Router&) is intentionally NOT in the concept: it is
     * templated on the connection type and checked where the listener pairs a
     * driver with a connection (later PR). The build/buy line is inside serve().
     */
    template <typename T>
    concept IsHttpDriver = requires {
        { T::id() } -> std::same_as<Protocol>;
        { T::accepted_alpns() } -> std::same_as<std::span<const std::string_view>>;
    };

}  // namespace demiplane::http
