#pragma once
#include <utility>

namespace demiplane::db {
    enum class Providers {
        None,
#ifdef POSTGRESQL_ENABLED
        PostgreSQL,
#endif
#ifdef REDIS_ENABLED
        Redis
#endif
    };

    constexpr const char* to_string(const Providers provider) noexcept {
        switch (provider) {
            case Providers::None:
                return "None";
#ifdef POSTGRESQL_ENABLED
            case Providers::PostgreSQL:
                return "PostgreSQL";
#endif
#ifdef REDIS_ENABLED
            case Providers::Redis:
                return "Redis";
#endif
        }
        std::unreachable();
    }
}  // namespace demiplane::db
