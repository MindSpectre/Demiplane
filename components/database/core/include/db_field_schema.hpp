#pragma once
#include <gears_types.hpp>
#include <gears_utils.hpp>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <boost/container/flat_map.hpp>
namespace demiplane::db {
    // Forward declarations
    template <typename T>
    class Column;

    template <>
    class Column<void>;

    // Enhanced FieldSchema with C++ type information
    struct FieldSchema {
        std::string name;
        std::string db_type;                                      // e.g., "VARCHAR(255)", "INTEGER", "TIMESTAMP"
        std::type_index cpp_type = std::type_index(typeid(void)); // C++ type info
        bool is_nullable         = true;
        bool is_primary_key      = false;
        bool is_foreign_key      = false;
        bool is_unique           = false;
        bool is_indexed          = false;
        std::string foreign_table; // For FK relationships
        std::string foreign_column;
        std::string default_value;
        std::size_t max_length = 0;

        // Database-specific attributes
        boost::container::flat_map<std::string, std::string> db_attributes;

        // Create typed column reference
        template <typename T>
        Column<T> as_column(std::shared_ptr<std::string> table) const {
            // Type safety check
            if (cpp_type != std::type_index(typeid(void)) &&
                cpp_type != std::type_index(typeid(T))) {
                throw std::logic_error("Type mismatch: field " + name +
                                       " expects " + gears::get_type_name_from_index(cpp_type) +
                                       " but got " + gears::get_type_name<T>());
            }
            return Column<T>{this, std::move(table)};
        }

        template <typename T>
        Column<T> as_column(std::string table) const {
            // Type safety check
            if (cpp_type != std::type_index(typeid(void)) &&
                cpp_type != std::type_index(typeid(T))) {
                throw std::logic_error("Type mismatch: field " + name +
                                       " expects " + gears::get_type_name_from_index(cpp_type) +
                                       " but got " + gears::get_type_name<T>());
            }
            return Column<T>{this, std::move(table)};
        }
    };
} // namespace demiplane::db
