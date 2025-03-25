#pragma once

#include "db_base.hpp"
#include "ires.hpp"
namespace demiplane::database {
    class TableTrait {
    public:
        virtual ~TableTrait() = default;
        // Table Management
        virtual IRes<> create_table(const query::CreateQuery& proposal) = 0;

        virtual IRes<> delete_table(std::string_view table_name) = 0;

        //todo: queries for cascade delete
        virtual IRes<> truncate_table(std::string_view table_name) = 0;

        [[nodiscard]] virtual IRes<bool> check_table(std::string_view table_name) = 0;

    };
} // namespace demiplane::database
