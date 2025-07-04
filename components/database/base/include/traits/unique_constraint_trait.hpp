#pragma once


#include "db_core.hpp"
#include "result.hpp"
namespace demiplane::database {
    namespace query {
        class SetUniqueConstraint final : public TableContext<SetUniqueConstraint>,
                                          public QueryUtilities<SetUniqueConstraint> {
        public:
            [[nodiscard]] const Columns& get_unique_columns() const {
                return indexed_columns_;
            }
            SetUniqueConstraint& make_constraint(Columns columns) {
                indexed_columns_ = std::move(columns);
                return *this;
            }

        private:
            Columns indexed_columns_;
        };
        class DeleteUniqueConstraint final : public TableContext<DeleteUniqueConstraint>,
                                             public QueryUtilities<DeleteUniqueConstraint> {
        public:
            DeleteUniqueConstraint() = default;
            explicit DeleteUniqueConstraint(const std::string_view table_name) : TableContext(table_name) {};
        };
    } // namespace query
    struct UniqueConstraintTrait {
        virtual ~UniqueConstraintTrait()                                                    = default;
        virtual gears::Result set_unique_constraint(const query::SetUniqueConstraint& query)       = 0;
        virtual gears::Result delete_unique_constraint(const query::DeleteUniqueConstraint& query) = 0;
    };
} // namespace demiplane::database
