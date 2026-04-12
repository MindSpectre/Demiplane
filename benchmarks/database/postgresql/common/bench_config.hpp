#pragma once

#include <cstdlib>
#include <format>
#include <string>

namespace bench::pg {

    inline std::string make_connection_string() {
        const char* host     = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "localhost";
        const char* port     = std::getenv("POSTGRES_PORT") ? std::getenv("POSTGRES_PORT") : "5433";
        const char* dbname   = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "test_db";
        const char* user     = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "test_user";
        const char* password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "test_password";

        return std::format("host={} port={} dbname={} user={} password={}", host, port, dbname, user, password);
    }

}  // namespace bench::pg
