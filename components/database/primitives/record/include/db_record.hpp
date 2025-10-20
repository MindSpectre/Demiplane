#pragma once

#include <boost/unordered/unordered_map.hpp>
#include <gears_hash.hpp>
#include <db_field.hpp>

namespace demiplane::db {

    class Table;

    /**
     * @brief Database record representing a single row with type-safe field access
     *
     * Record provides a container for database row data with efficient field access
     * patterns. Uses vector<Field> for storage with unordered_map for O(1) name-to-index
     * mapping. The schema is immutably shared via shared_ptr<const Table> ensuring
     * consistent field definitions across record instances.
     *
     * @attention Thread-unsafe. Concurrent access requires external synchronization.
     *                Multiple threads may safely read from different Record instances
     *                sharing the same schema simultaneously.
     *
     * @note Field access by name involves hash lookup (O(1) average, O(n) worst case).
     *       Field access by index is O(1) with bounds checking in debug builds.
     *       Iterator invalidation follows std::vector semantics - stable unless
     *       the underlying field vector is modified.
     */
    class Record final {
    public:
        /**
         * @brief Constructs a Record with the specified table schema
         * @param schema Table schema defining field structure and constraints
         * @throws std::invalid_argument if schema is null
         * @post All fields are initialized with default values according to schema
         */
        explicit Record(std::shared_ptr<const Table> schema);

        // Copy and move constructors are automatically generated and safe

        /**
         * @brief Accesses field by name with automatic creation if missing
         * @param field_name Name of the field to access
         * @return Reference to the field
         * @throws std::runtime_error if field_name is not defined in schema
         * @note Uses hash-based lookup with O(1) average complexity
         */
        Field& operator[](std::string_view field_name);

        /**
         * @brief Accesses field by name (const version)
         * @param field_name Name of the field to access
         * @return Const reference to the field
         * @throws std::runtime_error if field_name is not defined in schema
         * @note Uses hash-based lookup with O(1) average complexity
         */
        const Field& operator[](std::string_view field_name) const;

        /**
         * @brief Accesses field by index with bounds checking
         * @param index Zero-based field index
         * @return Reference to the field at specified index
         * @throws std::out_of_range if index >= field_count()
         * @complexity O(1)
         */
        Field& at(std::size_t index);

        /**
         * @brief Accesses field by index with bounds checking (const version)
         * @param index Zero-based field index
         * @return Const reference to the field at specified index
         * @throws std::out_of_range if index >= field_count()
         * @complexity O(1)
         */
        [[nodiscard]] const Field& at(std::size_t index) const;

        /**
         * @brief Safe field access by name returning nullable pointer
         * @param name Name of the field to access
         * @return Pointer to field if found, nullptr otherwise
         * @note Non-throwing alternative to operator[]
         */
        Field* get_field(std::string_view name);

        /**
         * @brief Safe field access by name returning nullable pointer (const version)
         * @param name Name of the field to access
         * @return Const pointer to field if found, nullptr otherwise
         * @note Non-throwing alternative to operator[]
         */
        [[nodiscard]] const Field* get_field(std::string_view name) const;

        /**
         * @brief Returns reference to the table schema
         * @return Const reference to the associated Table schema
         * @note Lifetime tied to shared_ptr held by this Record
         */
        [[nodiscard]] constexpr const Table& schema() const {
            return *schema_;
        }
        /**
         * @brief Returns shared pointer to the table schema
         * @return Shared pointer to the associated Table schema
         * @note Allows sharing schema ownership across multiple Records
         */
        [[nodiscard]] constexpr std::shared_ptr<const Table> table_ptr() const {
            return schema_;
        }

        /**
         * @brief Returns the number of fields in this record
         * @return Total field count as defined by schema
         * @complexity O(1)
         */
        [[nodiscard]] constexpr std::size_t field_count() const {
            return fields_.size();
        }

        /**
         * @brief Returns iterator to the first field
         * @return Mutable iterator to beginning of field vector
         * @note Iterator invalidation follows std::vector semantics
         */
        constexpr std::vector<Field>::iterator begin() {
            return fields_.begin();
        }

        /**
         * @brief Returns iterator past the last field
         * @return Mutable iterator to end of field vector
         * @note Iterator invalidation follows std::vector semantics
         */
        constexpr std::vector<Field>::iterator end() {
            return fields_.end();
        }

        /**
         * @brief Returns const iterator to the first field
         * @return Const iterator to beginning of field vector
         * @note Iterator invalidation follows std::vector semantics
         */
        [[nodiscard]] constexpr std::vector<Field>::const_iterator begin() const {
            return fields_.begin();
        }

        /**
         * @brief Returns const iterator past the last field
         * @return Const iterator to end of field vector
         * @note Iterator invalidation follows std::vector semantics
         */
        [[nodiscard]] constexpr std::vector<Field>::const_iterator end() const {
            return fields_.end();
        }

    private:
        std::shared_ptr<const Table> schema_;
        std::vector<Field> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };
}  // namespace demiplane::db
