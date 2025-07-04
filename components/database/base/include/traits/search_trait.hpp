#pragma once

#include "db_core.hpp"
#include "result.hpp"

namespace demiplane::database {
    namespace query {
        class SetIndexQuery final : public TableContext<SetIndexQuery>, public QueryUtilities<SetIndexQuery> {
        public:
            [[nodiscard]] const Columns& get_indexed_columns() const {
                return indexed_columns_;
            }
            SetIndexQuery& index(Columns columns) {
                indexed_columns_ = std::move(columns);
                return *this;
            }

        private:
            Columns indexed_columns_;
        };

        class DropIndexQuery final : public TableContext<SetIndexQuery>, public QueryUtilities<SetIndexQuery> {
        public:
            DropIndexQuery() = default;
            explicit DropIndexQuery(const std::string_view table_name) : TableContext(table_name) {}
        };
    } // namespace query
    struct SearchTrait {
        virtual ~SearchTrait()                                                                    = default;
        [[nodiscard]] virtual gears::Result setup_search_index(const query::SetIndexQuery& query) = 0;
        [[nodiscard]] virtual gears::Result drop_search_index(const query::DropIndexQuery& query) = 0;
    };
} // namespace demiplane::database
