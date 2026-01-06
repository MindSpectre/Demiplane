#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>

namespace demiplane::db::postgres {

    enum class NodeRole {
        PRIMARY,
        STANDBY_SYNC,   // Synchronous standby
        STANDBY_ASYNC,  // Asynchronous standby
        ANALYTICS,      // Read-only for analytics
        ARCHIVE         // Historical data
    };

    enum class SslMode { DISABLE, ALLOW, PREFER, REQUIRE, VERIFY_CA, VERIFY_FULL };

    enum class TransactionIsolation { READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };

    struct ConnectionConfig {
        // Basic connection
        std::string host{"localhost"};
        std::string port{"5432"};
        std::string dbname;
        std::string user;
        std::string password;

        // Node information
        NodeRole role{NodeRole::PRIMARY};
        int priority{0};  // For replica selection (higher = preferred)
        std::string cluster_name;

        // Timeouts
        std::chrono::seconds connect_timeout{30};
        std::chrono::seconds statement_timeout{0};
        std::chrono::seconds idle_in_transaction_timeout{0};
        std::chrono::seconds lock_timeout{0};

        // SSL/TLS
        SslMode ssl_mode{SslMode::PREFER};
        std::optional<std::string> ssl_cert;
        std::optional<std::string> ssl_key;
        std::optional<std::string> ssl_root_cert;

        // Protocol settings
        bool binary_protocol{true};
        bool auto_prepare{false};
        bool pipeline_mode{true};  // Use if PG14+
        std::string application_name;
        std::string search_path{"public"};

        // Performance
        size_t work_mem_mb{4};  // Per operation memory
        bool jit{true};         // JIT compilation for queries

        // Additional libpq options
        std::map<std::string, std::string> extra_options;

        // Generate libpq connection string
        [[nodiscard]] std::string to_connection_string() const;

        // Validate config
        [[nodiscard]] bool validate() const;
    };
}  // namespace demiplane::db::postgres
