#pragma once
#include <concepts>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include <boost/unordered_map.hpp>
#include <db_field_def.hpp>
#include <db_field_schema.hpp>
#include <db_schema_member.hpp>
#include <gears_concepts.hpp>
#include <gears_hash.hpp>

namespace demiplane::db {

    enum class SupportedProviders : std::uint8_t;
    // Forward declarations
    template <typename T>
    class TableColumn;

    class SqlDialect;  // Forward declaration - keeps primitives as base layer

    // Concept for schema types that have a fields type_list
    template <typename T>
    concept HasSchemaFields = requires { typename T::fields; };

    class Table {
    public:
        template <gears::IsStringLike StringTp>
        constexpr explicit Table(StringTp&& table_name) noexcept
            : table_name_{std::forward<StringTp>(table_name)} {
        }
        // ✨ Schema-aware constructor - auto-initializes fields from Schema::fields
        template <HasSchemaFields Schema>
        explicit Table(std::string table_name, Schema schema);

        // Enhanced builder pattern with type information
        template <typename T>
        Table& add_field(std::string name, std::string db_type);

        // Overload for runtime type specification
        Table& add_field(std::string name, std::string db_type, std::type_index cpp_type);

        // ═══════════════════════════════════════════════════════════════
        // TYPE-INFERRED ADD_FIELD - SQL type derived from C++ type
        // ═══════════════════════════════════════════════════════════════

        // Infer SQL type from dialect reference
        template <typename T>
        Table& add_field(std::string name, const SqlDialect& dialect);

        // Infer SQL type from dialect pointer
        template <typename T>
        Table& add_field(std::string name, const SqlDialect* dialect);

        // Infer SQL type from provider enum (compile-time)
        template <typename T>
        Table& add_field(std::string name, SupportedProviders provider);

        // ═══════════════════════════════════════════════════════════════
        // COLUMN ACCESSORS - Runtime and Compile-Time Overloads
        // ═══════════════════════════════════════════════════════════════

        // Runtime type-safe column accessor (existing API)
        template <typename T>
        [[nodiscard]] TableColumn<T> column(std::string_view field_name) const;

        // ✨ COMPILE-TIME type-safe column accessor (new API)
        template <IsFieldDef FieldDefT>
        [[nodiscard]] constexpr TableColumn<typename FieldDefT::value_type> column(FieldDefT field_def) const;


        // ═══════════════════════════════════════════════════════════════
        // BUILDER METHODS - Runtime and Compile-Time Overloads
        // ═══════════════════════════════════════════════════════════════

        // Runtime builders (existing API)
        Table& primary_key(std::string_view field_name);
        Table& nullable(std::string_view field_name, bool is_null = true);
        Table& foreign_key(std::string_view field_name, std::string_view ref_table, std::string_view ref_column);
        Table& unique(std::string_view field_name);
        Table& indexed(std::string_view field_name);

        // ✨ COMPILE-TIME builders (new API - FieldDef overloads)
        template <IsFieldDef FieldDefT>
        Table& primary_key(FieldDefT field_def);

        template <IsFieldDef FieldDefT>
        Table& nullable(FieldDefT field_def, bool is_null = true);

        template <IsFieldDef FieldDefT>
        Table& foreign_key(FieldDefT field_def, std::string_view ref_table, std::string_view ref_column);

        template <IsFieldDef FieldDefT>
        Table& unique(FieldDefT field_def);

        template <IsFieldDef FieldDefT>
        Table& indexed(FieldDefT field_def);

        // Set database type for a field (compile-time overload)
        template <IsFieldDef FieldDefT>
        Table& set_db_type(FieldDefT field_def, std::string db_type);

        // Add database-specific attributes (compile-time overload)
        template <IsFieldDef FieldDefT>
        Table& add_db_attribute(FieldDefT field_def, std::string key, std::string value);

        [[nodiscard]] const FieldSchema* get_field_schema(std::string_view name) const;
        FieldSchema* get_field_schema(std::string_view name);

        [[nodiscard]] constexpr const std::string& table_name() const {
            return table_name_;
        }

        [[nodiscard]] constexpr std::size_t field_count() const {
            return fields_.size();
        }

        [[nodiscard]] constexpr const std::vector<std::unique_ptr<FieldSchema>>& fields() const {
            return fields_;
        }

        // Get all column names
        [[nodiscard]] std::vector<std::string> field_names() const;

        [[nodiscard]] std::shared_ptr<Table> clone();

        [[nodiscard]] static std::shared_ptr<Table> make_ptr(std::string name) {
            return std::make_shared<Table>(std::move(name));
        }

        // ✨ Create Table from Schema (extracts table_name automatically)
        template <HasSchemaInfo Schema>
        [[nodiscard]] static std::shared_ptr<Table> make() {
            return std::make_shared<Table>(std::string(Schema::table_name), Schema{});
        }

    private:
        std::string table_name_;
        std::vector<std::unique_ptr<FieldSchema>> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
    };

    using TablePtr = std::shared_ptr<const Table>;

    template <typename TablePtrT>
    concept IsTablePtr = std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrT>>;

    template <typename TableT>
    concept IsTable =
        IsTablePtr<std::remove_cvref_t<TableT>> || std::constructible_from<std::string, std::remove_cvref_t<TableT>> ||
        std::constructible_from<std::string_view, std::remove_cvref_t<TableT>>;

}  // namespace demiplane::db

#include "../source/db_table.inl"
