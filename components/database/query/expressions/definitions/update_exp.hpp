#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsCondition Condition>
    class UpdateWhereExpr;

    class UpdateExpr : public Expression<UpdateExpr> {
    public:
        explicit UpdateExpr(TableSchemaPtr t)
            : table_(std::move(t)) {}

        // Set single column
        UpdateExpr& set(const std::string& column, FieldValue value) {
            assignments_.emplace_back(column, std::move(value));
            return *this;
        }

        // Set multiple columns
        UpdateExpr& set(const std::initializer_list<std::pair<std::string, FieldValue>> assigns) {
            for (auto& a : assigns) {
                assignments_.push_back(a);
            }
            return *this;
        }

        // WHERE clause
        template <IsCondition Condition>
        auto where(Condition cond) const {
            return UpdateWhereExpr<Condition>{*this, std::move(cond)};
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

        [[nodiscard]] const std::vector<std::pair<std::string, FieldValue>>& assignments() const {
            return assignments_;
        }

    private:
        TableSchemaPtr table_{nullptr};
        std::vector<std::pair<std::string, FieldValue>> assignments_;
    };

    inline auto update(TableSchemaPtr table) {
        return UpdateExpr{std::move(table)};
    }

    inline auto update(const std::string_view table_name) {
        auto schema = std::make_shared<const TableSchema>(table_name);
        return UpdateExpr{std::move(schema)};
    }
}
