#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace demiplane::db::postgres {
    // TODO: Perhaps should be placed somewhere else and reused for creating other identifiers(not only savepoints)
    /// Validates that `name` is a safe PostgreSQL identifier.
    /// Allowed: [a-zA-Z_][a-zA-Z0-9_]*, max 63 chars.
    [[nodiscard]] constexpr bool is_valid_identifier(std::string_view name) noexcept {
        if (name.empty() || name.size() > 63)
            return false;
        if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_')
            return false;
        return std::ranges::all_of(
            name, [](const char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
    }

}  // namespace demiplane::db::postgres
