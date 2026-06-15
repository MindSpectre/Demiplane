#include "request_arena.hpp"

// Header-only logic; this TU exists so the leaf is a STATIC archive with a
// stable object, matching the per-leaf convention.
namespace demiplane::http {
    namespace {
        [[maybe_unused]] constexpr int request_arena_tu_anchor = 0;
    }
}  // namespace demiplane::http
