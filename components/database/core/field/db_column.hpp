#pragma once

#include <algorithm>
#include <string>

#include "db_field.hpp"

namespace demiplane::database {
    class Column final {
    public:
        template <typename T>
        Column(std::string name, T value) : column_name_(std::move(name)) {
            sql_type_ = detail::deduce_sql_type<T>(value);
        }
        explicit Column(std::string name, const SqlType sqt) : column_name_(std::move(name)) {
            sql_type_ = sqt;
        }
        [[nodiscard]] const std::string& get_column_name() const {
            return column_name_;
        }
        void set_column_name(std::string column_name) {
            column_name_ = std::move(column_name);
        }
        [[nodiscard]] SqlType get_sql_type() const {
            return sql_type_;
        }
        [[nodiscard]] std::string get_sql_type_initialization() const {
            return detail::get_sql_init_type(sql_type_);
        }

    protected:
        SqlType sql_type_{SqlType::UNSUPPORTED};
        std::string column_name_;
    };
} // namespace demiplane::database
