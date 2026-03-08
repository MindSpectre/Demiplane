#pragma once

#include <demiplane/gears>

#include <boost/unordered_map.hpp>
#include <providers.hpp>
#include <schema/db_dynamic_field_schema.hpp>

namespace demiplane::db {
    // Forward declarations
    class Column;

    class DynamicTable {
    public:
        template <gears::IsStringLike StringTp>
        constexpr explicit DynamicTable(StringTp&& table_name) noexcept
            : table_name_{std::forward<StringTp>(table_name)} {
        }

        // Constructor with provider enum
        template <gears::IsStringLike StringTp>
        constexpr explicit DynamicTable(StringTp&& table_name, const Providers provider) noexcept
            : table_name_{std::forward<StringTp>(table_name)},
              provider_{provider} {
        }

        // Enhanced builder pattern with type information
        template <typename T, gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
        DynamicTable& add_field(StringTp1&& name, StringTp2&& db_type);

        // Overload for runtime type specification
        template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
        DynamicTable& add_field(StringTp1&& name, StringTp2&& db_type, std::type_index cpp_type);

        // Infer SQL type from stored provider - throws if provider not set
        template <typename T, gears::IsStringLike StringTp>
        DynamicTable& add_field(StringTp&& name);

        // Runtime column accessor — returns untyped Column
        [[nodiscard]] Column column(std::string_view field_name) const;

        // Runtime builders
        DynamicTable& primary_key(std::string_view field_name);
        DynamicTable& nullable(std::string_view field_name, bool is_null = true);
        DynamicTable& foreign_key(std::string_view field_name, std::string_view ref_table, std::string_view ref_column);
        DynamicTable& unique(std::string_view field_name);
        DynamicTable& indexed(std::string_view field_name);

        [[nodiscard]] const DynamicFieldSchema* get_field_schema(std::string_view name) const;
        DynamicFieldSchema* get_field_schema(std::string_view name);

        [[nodiscard]] constexpr const std::string& table_name() const {
            return table_name_;
        }

        [[nodiscard]] constexpr std::size_t field_count() const {
            return fields_.size();
        }

        [[nodiscard]] constexpr const std::vector<std::unique_ptr<DynamicFieldSchema>>& fields() const {
            return fields_;
        }

        [[nodiscard]] constexpr Providers provider() const noexcept {
            return provider_;
        }

        // Get all column names
        [[nodiscard]] std::vector<std::string> field_names() const;

        [[nodiscard]] std::shared_ptr<DynamicTable> clone();

        [[nodiscard]] static std::shared_ptr<DynamicTable> make_ptr(std::string name) {
            return std::make_shared<DynamicTable>(std::move(name));
        }

    private:
        std::string table_name_;
        std::vector<std::unique_ptr<DynamicFieldSchema>> fields_;
        boost::unordered_map<std::string, std::size_t, gears::StringHash, gears::StringEqual> field_index_;
        Providers provider_ = Providers::None;
    };
}  // namespace demiplane::db

#include "detail/db_dynamic_table.inl"
