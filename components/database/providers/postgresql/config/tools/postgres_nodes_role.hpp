#pragma once

namespace demiplane::db::postgres {
    enum class NodeRole {
        PRIMARY,
        STANDBY_SYNC,   // Synchronous standby
        STANDBY_ASYNC,  // Asynchronous standby
        ANALYTICS,      // Read-only for analytics
        ARCHIVE         // Historical data
    };

    [[nodiscard]] constexpr std::string_view node_role_to_string(const NodeRole role) noexcept {
        switch (role) {
            case NodeRole::PRIMARY:
                return "primary";
            case NodeRole::STANDBY_SYNC:
                return "standby_sync";
            case NodeRole::STANDBY_ASYNC:
                return "standby_async";
            case NodeRole::ANALYTICS:
                return "analytics";
            case NodeRole::ARCHIVE:
                return "archive";
        }
        std::unreachable();
    }

    template <typename T>
        requires std::is_integral_v<T>
    [[nodiscard]] constexpr static NodeRole node_role_from_int(const T value) noexcept {
        switch (value) {
            case 0:
                return NodeRole::PRIMARY;
            case 1:
                return NodeRole::STANDBY_SYNC;
            case 2:
                return NodeRole::STANDBY_ASYNC;
            case 3:
                return NodeRole::ANALYTICS;
            case 4:
                return NodeRole::ARCHIVE;
            default:
                return NodeRole::PRIMARY;
        }
        std::unreachable();
    }


}  // namespace demiplane::db::postgres
