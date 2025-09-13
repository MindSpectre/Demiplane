#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    class DeleteExpr : public Expression<DeleteExpr> {
    public:
        constexpr explicit DeleteExpr(TablePtr t)
            : table_(std::move(t)) {
        }

        template <IsCondition Condition>
        [[nodiscard]] auto where(Condition cond) const {
            return DeleteWhereExpr<Condition>{*this, std::move(cond)};
        }

        [[nodiscard]] const TablePtr& table() const {
            return table_;
        }

    private:
        TablePtr table_;
    };

    inline auto delete_from(TablePtr table) {
        return DeleteExpr{std::move(table)};
    }

    inline auto delete_from(std::string table_name) {
        return DeleteExpr{Table::make_ptr(std::move(table_name))};
    }
}  // namespace demiplane::db
