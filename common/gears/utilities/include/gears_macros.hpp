#pragma once

#include <utility>

#include "gears_templates.hpp"

#define CAT_(A, B) A##B
#define CAT(A, B) CAT_(A, B)  // ← forces expansion of arguments first

#define GEARS_UNREACHABLE(TYPE, TEXT)                                                                                  \
    static_assert(demiplane::gears::dependent_false_v<TYPE>, TEXT);                                                    \
    std::unreachable();
