#pragma once

#include <chrono>
#include <variant>

namespace demiplane::nexus {

    struct Flex        {};
    struct Immortal    {};
    struct Timed       { std::chrono::seconds idle{60}; };
    struct Scoped      {};

    using Lifetime = std::variant<Flex, Scoped, Timed, Immortal>;

} // namespace demiplane::nexus