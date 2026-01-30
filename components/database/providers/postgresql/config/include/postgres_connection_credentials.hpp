#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <gears_class_traits.hpp>
#include <gears_concepts.hpp>
#include <json/value.h>

namespace demiplane::db::postgres {

    /**
     * @brief Minimal PostgreSQL connection credentials
     *
     * Contains only the essential parameters needed to establish a database connection.
     * Suitable for simple executor creation.
     *
     * Usage:
     *   auto creds = ConnectionCredentials{}
     *       .host("localhost")
     *       .port(5432)
     *       .dbname("mydb")
     *       .user("admin")
     *       .password("secret")
     *       .finalize();
     *
     *   PGconn* conn = PQconnectdb(creds.to_connection_string().c_str());
     */
    class ConnectionCredentials final : public gears::ConfigInterface<ConnectionCredentials, Json::Value> {
    public:
        constexpr ConnectionCredentials() = default;

        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2, gears::IsStringLike StringTp3,
                  gears::IsStringLike StringTp4, gears::IsStringLike StringTp5>
        constexpr ConnectionCredentials(
            StringTp1&& host, StringTp2&& port, StringTp3&& dbname, StringTp4&& user, StringTp5&& password)
            : host_{std::forward<StringTp1>(host)},
              port_{std::forward<StringTp2>(port)},
              dbname_{std::forward<StringTp3>(dbname)},
              user_{std::forward<StringTp4>(user)},
              password_{std::forward<StringTp5>(password)} {
        }

        // ============== ConfigInterface Implementation ==============

        constexpr void validate() const override {
            if (dbname_.empty()) {
                throw std::invalid_argument("Database name (dbname) must be specified");
            }
            if (user_.empty()) {
                throw std::invalid_argument("User must be specified");
            }

            if (!port_.empty()) {
                try {
                    if (const int p = std::stoi(port_); p < 1 || p > 65535) {
                        throw std::invalid_argument("Port must be between 1 and 65535");
                    }
                } catch (const std::invalid_argument&) {
                    throw;
                } catch (const std::exception&) {
                    throw std::invalid_argument("Invalid port number");
                }
            }
        }

        // ============== Fluent Setters ==============

        template <typename Self>
        constexpr auto&& host(this Self&& self, std::string value) noexcept {
            self.host_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, std::string value) noexcept {
            self.port_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& port(this Self&& self, std::uint16_t value) noexcept {
            self.port_ = std::to_string(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& dbname(this Self&& self, std::string value) noexcept {
            self.dbname_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& user(this Self&& self, std::string value) noexcept {
            self.user_ = std::move(value);
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr auto&& password(this Self&& self, std::string value) noexcept {
            self.password_ = std::move(value);
            return std::forward<Self>(self);
        }

        // ============== Getters ==============

        [[nodiscard]] constexpr std::string_view host() const noexcept {
            return host_;
        }
        [[nodiscard]] constexpr std::string_view port() const noexcept {
            return port_;
        }
        [[nodiscard]] constexpr std::string_view dbname() const noexcept {
            return dbname_;
        }
        [[nodiscard]] constexpr std::string_view user() const noexcept {
            return user_;
        }
        [[nodiscard]] constexpr std::string_view password() const noexcept {
            return password_;
        }

        // ============== Connection String ==============

        /**
         * @brief Generate libpq connection string
         * @return Connection string in "key=value key=value" format
         */
        [[nodiscard]] constexpr std::string to_connection_string() const {
            validate();
            std::string result;
            result.reserve(256);

            if (!host_.empty()) {
                result += "host=" + escape_connection_value(host_) + " ";
            }
            if (!port_.empty()) {
                result += "port=" + escape_connection_value(port_) + " ";
            }
            if (!dbname_.empty()) {
                result += "dbname=" + escape_connection_value(dbname_) + " ";
            }
            if (!user_.empty()) {
                result += "user=" + escape_connection_value(user_) + " ";
            }
            if (!password_.empty()) {
                result += "password=" + escape_connection_value(password_) + " ";
            }

            // Remove trailing space
            if (!result.empty() && result.back() == ' ') {
                result.pop_back();
            }

            return result;
        }

        // ============== Equality ==============

        [[nodiscard]] bool operator==(const ConnectionCredentials& other) const noexcept {
            return host_ == other.host_ && port_ == other.port_ && dbname_ == other.dbname_ && user_ == other.user_ &&
                   password_ == other.password_;
        }

    protected:
        [[nodiscard]] Json::Value wrapped_serialize() const override;
        void wrapped_deserialize(const Json::Value& config) override;

    private:
            /**
             * @brief Escape a value for libpq connection string
             *
             * Rules per libpq documentation:
             * - Backslash and single quote must be escaped with backslash
             * - If value contains spaces or special chars, wrap in single quotes
             */
            [[nodiscard]] static std::string escape_connection_value(const std::string_view value) {
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


        std::string host_ = "localhost";
        std::string port_ = "5432";
        std::string dbname_;
        std::string user_;
        std::string password_;

    };

}  // namespace demiplane::db::postgres
