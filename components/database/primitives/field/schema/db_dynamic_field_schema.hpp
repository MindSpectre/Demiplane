#pragma once
#include <string>

#include <boost/container/flat_map.hpp>

namespace demiplane::db {

    /**
     * @brief Untyped database field metadata base struct
     *
     * Contains all SQL schema information and constraint definitions
     * without any C++ type mapping. Serves as the base for typed
     * field schema specializations.
     */
    struct DynamicFieldSchema {
        std::string name;             ///< Column name
        std::string db_type;          ///< SQL type (e.g., "VARCHAR(255)", "INTEGER")
        bool is_nullable    = true;   ///< NULL constraint
        bool is_primary_key = false;  ///< Primary key constraint
        bool is_foreign_key = false;  ///< Foreign key constraint
        bool is_unique      = false;  ///< Unique constraint
        bool is_indexed     = false;  ///< Index hint
        std::string foreign_table;    ///< FK target table
        std::string foreign_column;   ///< FK target column
        std::string default_value;    ///< Default value
        std::size_t max_length = 0;   ///< Maximum length for strings

        /// Database-specific attributes (e.g., "COLLATE", "CHECK")
        boost::container::flat_map<std::string, std::string> db_attributes;
    };

}  // namespace demiplane::db
