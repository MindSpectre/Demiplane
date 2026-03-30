#pragma once
#include <format>
#include <string>

namespace demiplane::db::detail {

    /**
     * @brief Escape a value for libpq connection string
     *
     * Rules per libpq documentation:
     * - Backslash and single quote must be escaped with backslash
     * - If value contains spaces or special chars, wrap in single quotes
     */
    constexpr std::string escape_connection_value(const std::string_view value) {
        if (value.empty()) {
            return "''";
        }

        bool needs_quoting = false;
        std::string escaped;
        escaped.reserve(value.size() + 2);

        for (const char c : value) {
            if (c == '\\' || c == '\'') {
                escaped       += '\\';
                needs_quoting  = true;
            } else if (c == ' ' || c == '=') {
                needs_quoting = true;
            }
            escaped += c;
        }

        if (needs_quoting) {
            return std::format("'{}'", escaped);
        }
        return escaped;
    }
}  // namespace demiplane::db::detail
