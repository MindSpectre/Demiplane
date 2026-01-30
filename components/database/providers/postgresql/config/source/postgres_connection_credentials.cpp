#include "postgres_connection_credentials.hpp"

#include <format>

namespace demiplane::db::postgres {





    [[nodiscard]] Json::Value ConnectionCredentials::wrapped_serialize() const {
        Json::Value result;
        result["host"]   = host_;
        result["port"]   = port_;
        result["dbname"] = dbname_;
        result["user"]   = user_;
        // Password excluded from serialization for security
        return result;
    }

    void ConnectionCredentials::wrapped_deserialize(const Json::Value& config) {
        host(config.get("host", "localhost").asString());
        port(config.get("port", "5432").asString());
        dbname(config.get("dbname", "").asString());
        user(config.get("user", "").asString());
        password(config.get("password", "").asString());
    }



}  // namespace demiplane::db::postgres
