#pragma once
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>

#include <boost/container/flat_map.hpp>
#include <gears_types.hpp>


namespace demiplane::db {
    template <typename T>
    class TableColumn;

    /**
     * @brief Database field schema with type safety and constraint metadata
     *
     * Represents a database column with SQL schema information, C++ type mapping,
     * and constraint definitions for type-safe ORM operations.
     */
    struct FieldSchema {
        std::string name;                                          ///< Column name
        std::string db_type;                                       ///< SQL type (e.g., "VARCHAR(255)", "INTEGER")
        std::type_index cpp_type = std::type_index(typeid(void));  ///< Mapped C++ type
        bool is_nullable         = true;                           ///< NULL constraint
        bool is_primary_key      = false;                          ///< Primary key constraint
        bool is_foreign_key      = false;                          ///< Foreign key constraint
        bool is_unique           = false;                          ///< Unique constraint
        bool is_indexed          = false;                          ///< Index hint
        std::string foreign_table;                                 ///< FK target table
        std::string foreign_column;                                ///< FK target column
        std::string default_value;                                 ///< Default value
        std::size_t max_length = 0;                                ///< Maximum length for strings

        /// Database-specific attributes (e.g., "COLLATE", "CHECK")
        boost::container::flat_map<std::string, std::string> db_attributes;

        /**
         * @brief Create a type-safe column reference
         * @tparam T Expected C++ type
         * @param table Shared table name
         * @return TableColumn with type safety validation
         * @throws std::logic_error if type mismatch
         */
        template <typename T>
        [[nodiscard]] TableColumn<T> as_column(std::shared_ptr<std::string> table) const {
            // Type safety check
            if (cpp_type != std::type_index(typeid(void)) && cpp_type != std::type_index(typeid(T))) {
                throw std::logic_error("Type mismatch: field " + name + " expects " +
                                       gears::get_type_name_from_index(cpp_type) + " but got " +
                                       gears::get_type_name<T>());
            }
            return TableColumn<T>{this, std::move(table)};
        }

        /**
         * @brief Create a type-safe column reference
         * @tparam T Expected C++ type
         * @param table Table name
         * @return TableColumn with type safety validation
         * @throws std::logic_error if type mismatch
         */
        template <typename T>
        [[nodiscard]] TableColumn<T> as_column(std::string table) const {
            // Type safety check
            if (cpp_type != std::type_index(typeid(void)) && cpp_type != std::type_index(typeid(T))) {
                throw std::logic_error("Type mismatch: field " + name + " expects " +
                                       gears::get_type_name_from_index(cpp_type) + " but got " +
                                       gears::get_type_name<T>());
            }
            return TableColumn<T>{this, std::move(table)};
        }
    };
}  // namespace demiplane::db
