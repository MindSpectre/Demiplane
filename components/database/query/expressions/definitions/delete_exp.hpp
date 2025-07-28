#pragma once

#include <algorithm>

#include "db_table_schema.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <IsCondition Condition>
    class DeleteWhereExpr;

    class DeleteExpr : public Expression<DeleteExpr> {
    public:
        explicit DeleteExpr(TableSchemaPtr t)
            : table_(std::move(t)) {}

        // WHERE clause
        template <IsCondition Condition>
        auto where(Condition cond) const {
            return DeleteWhereExpr<Condition>{*this, std::move(cond)};
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

    private:
        TableSchemaPtr table_{nullptr};
    };
    inline auto delete_from(TableSchemaPtr table) {
        return DeleteExpr{std::move(table)};
    }

    inline auto delete_from(const std::string_view table_name) {
        auto schema = std::make_shared<const TableSchema>(table_name);
        return DeleteExpr{std::move(schema)};
    }
}