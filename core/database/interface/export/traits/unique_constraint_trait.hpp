#pragma once


#include "db_base.hpp"
#include "ires.hpp"
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
            SILENT_TABLE_CONSTRUCTOR(DeleteUniqueConstraint);
        };
    } // namespace query
    class UniqueConstraintTrait {
    public:
        virtual ~UniqueConstraintTrait()                                                    = default;
        virtual Result set_unique_constraint(const query::SetUniqueConstraint& query)      = 0;
        virtual Result delete_unique_constraint(const query::DeleteUniqueConstraint& query) = 0;
    };
} // namespace demiplane::database
