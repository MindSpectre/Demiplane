#pragma once

#include <algorithm>

#include "../basic.hpp"
#include "db_table_schema.hpp"

namespace demiplane::db {
    class DeleteExpr : public Expression<DeleteExpr> {
        public:
        constexpr explicit DeleteExpr(TableSchemaPtr t)
            : table_(std::move(t)) {
        }

        template <IsCondition Condition>
        auto where(Condition cond) const {
            return DeleteWhereExpr<Condition>{*this, std::move(cond)};
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

        private:
        TableSchemaPtr table_;
    };

    inline auto delete_from(TableSchemaPtr table) {
        return DeleteExpr{std::move(table)};
    }

    inline auto delete_from(std::string table_name) {
        return DeleteExpr{TableSchema::make_ptr(std::move(table_name))};
    }
}  // namespace demiplane::db
