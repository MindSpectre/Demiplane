#pragma once

#include <db_column.hpp>

namespace demiplane::db {

    // FIELD MANAGEMENT
    template <typename T, gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
    DynamicTable& DynamicTable::add_field(StringTp1&& name, StringTp2&& db_type) {
        auto field = std::make_unique<DynamicFieldSchema>();

        if constexpr (std::is_same_v<std::decay_t<StringTp1>, std::string>) {
            field->name = std::forward<StringTp1>(name);
        } else {
            field->name = std::string{name};
        }

        if constexpr (std::is_same_v<std::decay_t<StringTp2>, std::string>) {
            field->db_type = std::forward<StringTp2>(db_type);
        } else {
            field->db_type = std::string{db_type};
        }

        field_index_[field->name] = fields_.size();
        fields_.push_back(std::move(field));
        return *this;
    }

    template <gears::IsStringLike StringTp1, gears::IsStringLike StringTp2>
    DynamicTable&
    DynamicTable::add_field(StringTp1&& name, StringTp2&& db_type, [[maybe_unused]] const std::type_index cpp_type) {
        auto field = std::make_unique<DynamicFieldSchema>();

        if constexpr (std::is_same_v<std::decay_t<StringTp1>, std::string>) {
            field->name = std::forward<StringTp1>(name);
        } else {
            field->name = std::string{name};
        }

        if constexpr (std::is_same_v<std::decay_t<StringTp2>, std::string>) {
            field->db_type = std::forward<StringTp2>(db_type);
        } else {
            field->db_type = std::string{db_type};
        }

        field_index_[field->name] = fields_.size();
        fields_.push_back(std::move(field));
        return *this;
    }

}  // namespace demiplane::db
