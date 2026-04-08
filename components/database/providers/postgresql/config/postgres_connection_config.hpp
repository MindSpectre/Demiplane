#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <config_interface.hpp>
#include <json/json.hpp>

#include "credentials/postgres_connection_credentials.hpp"
#include "tools/postgres_connection_tools.hpp"
#include "tools/postgres_nodes_role.hpp"
#include "tools/postgres_ssl_mode.hpp"

namespace demiplane::db::postgres {

    enum class TransactionIsolation { READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };

    /**
     * @brief Full PostgreSQL connection configuration
     *
     * Contains ConnectionCredentials plus all advanced options for production deployments
     * including SSL, timeouts, node roles, and performance tuning.
     *
     * Suitable for Session class with LMAX disruptor pool.
     *
     * Usage:
     *   auto config = ConnectionConfig::Builder{}
     *       .host("db.example.com")
     *       .port(5432)
     *       .dbname("production")
     *       .user("app_user")
     *       .password("secret")
     *       .ssl_mode(SslMode::VERIFY_FULL)
     *       .ssl_root_cert("/path/to/ca.pem")
     *       .connect_timeout(std::chrono::seconds{30})
     *       .role(NodeRole::PRIMARY)
     *       .finalize();
     */
    class ConnectionConfig final : public serialization::ConfigInterface<ConnectionConfig, Json::Value> {
    public:
        explicit constexpr ConnectionConfig(ConnectionCredentials credentials) noexcept
            : credentials_{std::move(credentials)} {
        }

        // ============== ConfigInterface Implementation ==============

        constexpr void validate() const override {
            credentials_.validate();

            if ((ssl_mode_ == SslMode::VERIFY_CA || ssl_mode_ == SslMode::VERIFY_FULL) && !ssl_root_cert_.has_value()) {
                throw std::invalid_argument("ssl_root_cert is required when ssl_mode is VERIFY_CA or VERIFY_FULL");
            }

            if (ssl_cert_.has_value() != ssl_key_.has_value()) {
                throw std::invalid_argument("ssl_cert and ssl_key must both be specified or both be omitted");
            }

            if (connect_timeout_.count() < 0) {
                throw std::invalid_argument("connect_timeout cannot be negative");
            }

            if (priority_ < 0) {
                throw std::invalid_argument("priority cannot be negative");
            }
        }

        // ============== Credential Accessors ==============

        [[nodiscard]] constexpr const ConnectionCredentials& credentials() const noexcept {
            return credentials_;
        }

        // ============== Getters ==============

        // Credential getters (delegated)
        [[nodiscard]] constexpr std::string_view host() const noexcept {
            return credentials_.host();
        }
        [[nodiscard]] constexpr std::string_view port() const noexcept {
            return credentials_.port();
        }
        [[nodiscard]] constexpr std::string_view dbname() const noexcept {
            return credentials_.dbname();
        }
        [[nodiscard]] constexpr std::string_view user() const noexcept {
            return credentials_.user();
        }
        [[nodiscard]] constexpr std::string_view password() const noexcept {
            return credentials_.password();
        }

        // Node configuration
        [[nodiscard]] constexpr NodeRole role() const noexcept {
            return role_;
        }
        [[nodiscard]] constexpr int priority() const noexcept {
            return priority_;
        }
        [[nodiscard]] constexpr std::string_view cluster_name() const noexcept {
            return cluster_name_;
        }

        // Timeouts
        [[nodiscard]] constexpr std::chrono::seconds connect_timeout() const noexcept {
            return connect_timeout_;
        }
        [[nodiscard]] constexpr std::chrono::seconds statement_timeout() const noexcept {
            return statement_timeout_;
        }
        [[nodiscard]] constexpr std::chrono::seconds idle_in_transaction_timeout() const noexcept {
            return idle_in_transaction_timeout_;
        }
        [[nodiscard]] constexpr std::chrono::seconds lock_timeout() const noexcept {
            return lock_timeout_;
        }

        // SSL/TLS
        [[nodiscard]] constexpr SslMode ssl_mode() const noexcept {
            return ssl_mode_;
        }
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_cert() const noexcept {
            return ssl_cert_;
        }
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_key() const noexcept {
            return ssl_key_;
        }
        [[nodiscard]] constexpr const std::optional<std::string>& ssl_root_cert() const noexcept {
            return ssl_root_cert_;
        }

        // Protocol
        [[nodiscard]] constexpr bool binary_protocol() const noexcept {
            return binary_protocol_;
        }
        [[nodiscard]] constexpr bool auto_prepare() const noexcept {
            return auto_prepare_;
        }
        [[nodiscard]] constexpr bool pipeline_mode() const noexcept {
            return pipeline_mode_;
        }
        [[nodiscard]] constexpr std::string_view application_name() const noexcept {
            return application_name_;
        }
        [[nodiscard]] constexpr std::string_view search_path() const noexcept {
            return search_path_;
        }

        // Performance
        [[nodiscard]] constexpr std::size_t work_mem_mb() const noexcept {
            return work_mem_mb_;
        }
        [[nodiscard]] constexpr bool jit() const noexcept {
            return jit_;
        }

        // Extra options
        [[nodiscard]] const std::map<std::string, std::string>& extra_options() const noexcept {
            return extra_options_;
        }

        // ============== Connection String ==============

        /**
         * @brief Generate full libpq connection string
         * @return Connection string including all configured options
         */
        [[nodiscard]] constexpr std::string to_connection_string() const {
            validate();
            std::string result = credentials_.to_connection_string();

            result += " sslmode=" + ssl_mode_to_string_t<std::string>(ssl_mode_);

            if (ssl_cert_) {
                result += " sslcert=" + detail::escape_connection_value(*ssl_cert_);
            }
            if (ssl_key_) {
                result += " sslkey=" + detail::escape_connection_value(*ssl_key_);
            }
            if (ssl_root_cert_) {
                result += " sslrootcert=" + detail::escape_connection_value(*ssl_root_cert_);
            }

            if (connect_timeout_.count() > 0) {
                result += " connect_timeout=" + std::to_string(connect_timeout_.count());
            }

            if (!application_name_.empty()) {
                result += " application_name=" + detail::escape_connection_value(application_name_);
            }

            for (const auto& [key, value] : extra_options_) {
                result += " " + key + "=" + detail::escape_connection_value(value);
            }

            return result;
        }

        // ============== Factory Methods (defined after Builder) ==============

        [[nodiscard]] static ConnectionConfig local_dev();
        [[nodiscard]] static ConnectionConfig testing();
        [[nodiscard]] static ConnectionConfig production();

        // ============== Field Descriptors ==============

        static constexpr auto fields() {
            return std::tuple{
                // Nested config (auto-serializes via HasFields overload)
                serialization::Field<&ConnectionConfig::credentials_, "credentials">{},
                // Node configuration
                serialization::Field<&ConnectionConfig::role_, "role">{},
                serialization::Field<&ConnectionConfig::priority_, "priority">{},
                serialization::Field<&ConnectionConfig::cluster_name_, "cluster_name">{},
                // Timeouts
                serialization::Field<&ConnectionConfig::connect_timeout_, "connect_timeout">{},
                serialization::Field<&ConnectionConfig::statement_timeout_, "statement_timeout">{},
                serialization::Field<&ConnectionConfig::idle_in_transaction_timeout_, "idle_in_transaction_timeout">{},
                serialization::Field<&ConnectionConfig::lock_timeout_, "lock_timeout">{},
                // SSL/TLS
                serialization::Field<&ConnectionConfig::ssl_mode_, "ssl_mode">{},
                serialization::Field<&ConnectionConfig::ssl_cert_, "ssl_cert">{},
                serialization::Field<&ConnectionConfig::ssl_key_, "ssl_key">{},
                serialization::Field<&ConnectionConfig::ssl_root_cert_, "ssl_root_cert">{},
                // Protocol
                serialization::Field<&ConnectionConfig::binary_protocol_, "binary_protocol">{},
                serialization::Field<&ConnectionConfig::auto_prepare_, "auto_prepare">{},
                serialization::Field<&ConnectionConfig::pipeline_mode_, "pipeline_mode">{},
                serialization::Field<&ConnectionConfig::application_name_, "application_name">{},
                serialization::Field<&ConnectionConfig::search_path_, "search_path">{},
                // Performance
                serialization::Field<&ConnectionConfig::work_mem_mb_, "work_mem_mb">{},
                serialization::Field<&ConnectionConfig::jit_, "jit">{},
                // Extra options
                serialization::Field<&ConnectionConfig::extra_options_, "extra_options">{},
            };
        }

        class Builder;

    private:
        friend class ConfigInterface;
        constexpr ConnectionConfig() = default;

        ConnectionCredentials credentials_;

        // Node configuration
        NodeRole role_ = NodeRole::PRIMARY;
        int priority_  = 0;
        std::string cluster_name_;

        // Timeouts
        std::chrono::seconds connect_timeout_             = std::chrono::seconds{30};
        std::chrono::seconds statement_timeout_           = std::chrono::seconds{0};
        std::chrono::seconds idle_in_transaction_timeout_ = std::chrono::seconds{0};
        std::chrono::seconds lock_timeout_                = std::chrono::seconds{0};

        // SSL/TLS
        SslMode ssl_mode_ = SslMode::PREFER;
        std::optional<std::string> ssl_cert_;
        std::optional<std::string> ssl_key_;
        std::optional<std::string> ssl_root_cert_;

        // Protocol settings
        bool binary_protocol_ = true;
        bool auto_prepare_    = false;
        bool pipeline_mode_   = true;
        std::string application_name_;
        std::string search_path_ = "public";

        // Performance
        std::size_t work_mem_mb_ = 4;
        bool jit_                = true;

        // Additional libpq options
        std::map<std::string, std::string> extra_options_;
    };

    class ConnectionConfig::Builder {
    public:
        Builder() = default;
        explicit Builder(const ConnectionConfig& existing) : config_{existing} {}
        explicit Builder(ConnectionConfig&& existing) : config_{std::move(existing)} {}

        // ============== Credential Setters (Proxied) ==============

        template <typename Self>
        constexpr auto&& credentials(this Self&& self, ConnectionCredentials creds) noexcept {
            self.config_.credentials_ = std::move(creds);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& host(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::move(value),
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, const std::uint16_t value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::to_string(value),
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& dbname(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.user()},
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& user(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::move(value),
                                                              std::string{self.config_.credentials_.password()}};
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& password(this Self&& self, std::string value) noexcept {
            self.config_.credentials_ = ConnectionCredentials{std::string{self.config_.credentials_.host()},
                                                              std::string{self.config_.credentials_.port()},
                                                              std::string{self.config_.credentials_.dbname()},
                                                              std::string{self.config_.credentials_.user()},
                                                              std::move(value)};
            return std::forward<Self>(self);
        }

        // ============== Node Configuration Setters ==============

        template <typename Self>
        constexpr auto&& role(this Self&& self, NodeRole value) noexcept {
            self.config_.role_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& priority(this Self&& self, int value) noexcept {
            self.config_.priority_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& cluster_name(this Self&& self, std::string value) noexcept {
            self.config_.cluster_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Timeout Setters ==============

        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& statement_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.statement_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle_in_transaction_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.idle_in_transaction_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& lock_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.config_.lock_timeout_ = value;
            return std::forward<Self>(self);
        }

        // ============== SSL/TLS Setters ==============

        template <typename Self>
        constexpr auto&& ssl_mode(this Self&& self, SslMode value) noexcept {
            self.config_.ssl_mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_cert(this Self&& self, std::string value) noexcept {
            self.config_.ssl_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_key(this Self&& self, std::string value) noexcept {
            self.config_.ssl_key_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_root_cert(this Self&& self, std::string value) noexcept {
            self.config_.ssl_root_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Protocol Setters ==============

        template <typename Self>
        constexpr auto&& binary_protocol(this Self&& self, bool value) noexcept {
            self.config_.binary_protocol_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& auto_prepare(this Self&& self, bool value) noexcept {
            self.config_.auto_prepare_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& pipeline_mode(this Self&& self, bool value) noexcept {
            self.config_.pipeline_mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& application_name(this Self&& self, std::string value) noexcept {
            self.config_.application_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& search_path(this Self&& self, std::string value) noexcept {
            self.config_.search_path_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Performance Setters ==============

        template <typename Self>
        constexpr auto&& work_mem_mb(this Self&& self, std::size_t value) noexcept {
            self.config_.work_mem_mb_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& jit(this Self&& self, bool value) noexcept {
            self.config_.jit_ = value;
            return std::forward<Self>(self);
        }

        // ============== Extra Options Setters ==============

        template <typename Self>
        constexpr auto&& extra_option(this Self&& self, std::string key, std::string value) noexcept {
            self.config_.extra_options_[std::move(key)] = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& extra_options(this Self&& self, std::map<std::string, std::string> options) noexcept {
            self.config_.extra_options_ = std::move(options);
            return std::forward<Self>(self);
        }

        [[nodiscard]] ConnectionConfig finalize() && {
            config_.validate();
            return std::move(config_);
        }

    private:
        friend class ConnectionConfig;
        friend class ConfigInterface;
        ConnectionConfig config_;
    };

    // ============== ConnectionConfig Factory Method Definitions ==============

    inline ConnectionConfig ConnectionConfig::local_dev() {
        return Builder{}
            .host("localhost")
            .port(static_cast<std::uint16_t>(5432))
            .ssl_mode(SslMode::DISABLE)
            .connect_timeout(std::chrono::seconds{5})
            .finalize();
    }

    inline ConnectionConfig ConnectionConfig::testing() {
        return Builder{}
            .host("localhost")
            .port(static_cast<std::uint16_t>(5433))
            .dbname("test_db")
            .user("test_user")
            .password("test_password")
            .ssl_mode(SslMode::DISABLE)
            .connect_timeout(std::chrono::seconds{10})
            .finalize();
    }

    inline ConnectionConfig ConnectionConfig::production() {
        return Builder{}
            .ssl_mode(SslMode::VERIFY_FULL)
            .connect_timeout(std::chrono::seconds{30})
            .statement_timeout(std::chrono::seconds{60})
            .pipeline_mode(true)
            .jit(true)
            .finalize();
    }

}  // namespace demiplane::db::postgres
