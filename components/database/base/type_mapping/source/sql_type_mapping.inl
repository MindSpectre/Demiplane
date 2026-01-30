#pragma once


#include <sql_dialect.hpp>

namespace demiplane::db {

    template <typename T>
    std::string_view sql_type(const SqlDialect& dialect) {
        return sql_type<T>(dialect.type());
    }

    template <typename T>
    std::string_view sql_type(const SqlDialect* dialect) {
        // Note: caller should ensure dialect is not null
        // A null dialect will cause undefined behavior (dereferencing nullptr)
        return sql_type<T>(dialect->type());
    }
}  // namespace demiplane::db
