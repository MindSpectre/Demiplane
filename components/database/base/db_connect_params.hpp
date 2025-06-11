#pragma once
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include "class_traits.hpp"

namespace demiplane::database {
    class ConnectParams : gears::Immovable {
    public:
        ConnectParams(
            std::string host, const uint32_t port, std::string db_name, std::string login, std::string password)
            : host_(std::move(host)), port_(port), db_name_(std::move(db_name)), login_(std::move(login)),
              password_(std::move(password)) {}

        ConnectParams() = default;

        [[nodiscard]] const std::string& get_host() const {
            return host_;
        }

        void set_host(const std::string& host) {
            host_ = host;
        }

        [[nodiscard]] uint32_t get_port() const {
            return port_;
        }

        void set_port(const uint32_t port) {
            port_ = port;
        }

        [[nodiscard]] const std::string& get_db_name() const {
            return db_name_;
        }

        void set_db_name(const std::string& db_name) {
            db_name_ = db_name;
        }

        [[nodiscard]] const std::string& get_login() const {
            return login_;
        }

        void set_login(const std::string& login) {
            login_ = login;
        }

        [[nodiscard]] const std::string& get_password() const {
            return password_;
        }

        void set_password(const std::string&& password) {
            password_ = password;
        }
        [[nodiscard]] std::string make_connect_string() const {
            std::ostringstream conn_str;
            conn_str << "host=" << this->host_ << " port=" << this->port_ << " user=" << this->login_
                     << " password=" << this->password_ << " dbname=" << this->db_name_;
            return conn_str.str();
        }

    private:
        bool valid = false;
        std::string host_{"localhost"};
        uint32_t port_{5432};
        std::string db_name_{"db_test"};
        std::string login_{"postgres"};
        std::string password_{"postgres"};
    };
} // namespace demiplane::database
