#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <gears_class_traits.hpp>
#include <json/value.h>

#include "credentials/postgres_connection_credentials.hpp"
#include "tools/postgres_nodes_role.hpp"
#include "tools/postgres_ssl_mode.hpp"
#include "tools/postgres_connection_tools.hpp"
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
     *   auto config = ConnectionConfig{}
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
    class ConnectionConfig final : public gears::ConfigInterface<ConnectionConfig, Json::Value> {
    public:
        constexpr ConnectionConfig() = default;

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

        [[nodiscard]] constexpr ConnectionCredentials& credentials() noexcept {
            return credentials_;
        }

        // ============== Fluent Setters for Credentials (Proxied) ==============

        template <typename Self>
        constexpr auto&& host(this Self&& self, std::string value) noexcept {
            self.credentials_ = std::move(self.credentials_).host(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, std::string value) noexcept {
            self.credentials_ = std::move(self.credentials_).port(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, std::uint16_t value) noexcept {
            self.credentials_ = std::move(self.credentials_).port(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& dbname(this Self&& self, std::string value) noexcept {
            self.credentials_ = std::move(self.credentials_).dbname(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& user(this Self&& self, std::string value) noexcept {
            self.credentials_ = std::move(self.credentials_).user(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& password(this Self&& self, std::string value) noexcept {
            self.credentials_ = std::move(self.credentials_).password(std::move(value));
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& credentials(this Self&& self, ConnectionCredentials creds) noexcept {
            self.credentials_ = std::move(creds);
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for Node Configuration ==============

        template <typename Self>
        constexpr auto&& role(this Self&& self, NodeRole value) noexcept {
            self.role_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& priority(this Self&& self, int value) noexcept {
            self.priority_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& cluster_name(this Self&& self, std::string value) noexcept {
            self.cluster_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for Timeouts ==============

        template <typename Self>
        constexpr auto&& connect_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.connect_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& statement_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.statement_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& idle_in_transaction_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.idle_in_transaction_timeout_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& lock_timeout(this Self&& self, std::chrono::seconds value) noexcept {
            self.lock_timeout_ = value;
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for SSL/TLS ==============

        template <typename Self>
        constexpr auto&& ssl_mode(this Self&& self, SslMode value) noexcept {
            self.ssl_mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_cert(this Self&& self, std::string value) noexcept {
            self.ssl_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_key(this Self&& self, std::string value) noexcept {
            self.ssl_key_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& ssl_root_cert(this Self&& self, std::string value) noexcept {
            self.ssl_root_cert_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for Protocol ==============

        template <typename Self>
        constexpr auto&& binary_protocol(this Self&& self, bool value) noexcept {
            self.binary_protocol_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& auto_prepare(this Self&& self, bool value) noexcept {
            self.auto_prepare_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& pipeline_mode(this Self&& self, bool value) noexcept {
            self.pipeline_mode_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& application_name(this Self&& self, std::string value) noexcept {
            self.application_name_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& search_path(this Self&& self, std::string value) noexcept {
            self.search_path_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for Performance ==============

        template <typename Self>
        constexpr auto&& work_mem_mb(this Self&& self, std::size_t value) noexcept {
            self.work_mem_mb_ = value;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& jit(this Self&& self, bool value) noexcept {
            self.jit_ = value;
            return std::forward<Self>(self);
        }

        // ============== Fluent Setters for Extra Options ==============

        template <typename Self>
        constexpr auto&& extra_option(this Self&& self, std::string key, std::string value) noexcept {
            self.extra_options_[std::move(key)] = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& extra_options(this Self&& self, std::map<std::string, std::string> options) noexcept {
            self.extra_options_ = std::move(options);
            return std::forward<Self>(self);
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

        // ============== Factory Methods ==============

        [[nodiscard]] constexpr static ConnectionConfig local_dev() {
            return ConnectionConfig{}
                .host("localhost")
                .port(5432)
                .ssl_mode(SslMode::DISABLE)
                .connect_timeout(std::chrono::seconds{5});
        }

        [[nodiscard]] constexpr static ConnectionConfig testing() {
            return ConnectionConfig{}
                .host("localhost")
                .port(5433)
                .dbname("test_db")
                .user("test_user")
                .password("test_password")
                .ssl_mode(SslMode::DISABLE)
                .connect_timeout(std::chrono::seconds{10});
        }

        [[nodiscard]] constexpr static ConnectionConfig production() {
            return ConnectionConfig{}
                .ssl_mode(SslMode::VERIFY_FULL)
                .connect_timeout(std::chrono::seconds{30})
                .statement_timeout(std::chrono::seconds{60})
                .pipeline_mode(true)
                .jit(true);
        }

    protected:
        [[nodiscard]] Json::Value wrapped_serialize() const override;
        void wrapped_deserialize(const Json::Value& config) override;

    private:

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

}  // namespace demiplane::db::postgres
