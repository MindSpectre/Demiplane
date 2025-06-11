#pragma once

#include "db_core.hpp"
#include "result.hpp"
namespace demiplane::database {
    namespace query {
        class CreateTableQuery final : public TableContext<CreateTableQuery>, public QueryUtilities<CreateTableQuery> {
        public:
            CreateTableQuery& columns(Columns columns) {
                columns_ = std::move(columns);
                return *this;
            }
            [[nodiscard]] const Columns& get_columns() const noexcept {
                return columns_;
            }

        private:
            Columns columns_;
        };
        class DropTableQuery final : public TableContext<DropTableQuery>, public QueryUtilities<DropTableQuery> {
        public:
            DropTableQuery() = default;
            explicit DropTableQuery(const std::string_view table_name) : TableContext(table_name) {}
        };
        class TruncateTableQuery final : public TableContext<TruncateTableQuery>,
                                         public QueryUtilities<TruncateTableQuery> {
        public:
            TruncateTableQuery() = default;
            explicit TruncateTableQuery(const std::string_view table_name) : TableContext(table_name) {}
        };
        class CheckTableQuery final : public TableContext<CheckTableQuery>, public QueryUtilities<CheckTableQuery> {
        public:
            CheckTableQuery() = default;
            explicit CheckTableQuery(const std::string_view table_name) : TableContext(table_name) {}
        };
    } // namespace query
    struct TableTrait {
        virtual ~TableTrait() = default;
        // Table Management
        virtual gears::Result create_table(const query::CreateTableQuery& query) = 0;

        virtual gears::Result drop_table(const query::DropTableQuery& query) = 0;

        // todo: queries for cascade delete
        virtual gears::Result truncate_table(const query::TruncateTableQuery& query) = 0;

        [[nodiscard]] virtual gears::Interceptor<bool> check_table(const query::CheckTableQuery& query) = 0;
    };
} // namespace demiplane::database
