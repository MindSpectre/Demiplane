#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    class UpdateExpr : public Expression<UpdateExpr> {
    public:
        explicit UpdateExpr(TableSchemaPtr t)
            : table_(std::move(t)) {}

        UpdateExpr& set(std::string column, FieldValue value) {
            assignments_.emplace_back(std::move(column), std::move(value));
            return *this;
        }

        UpdateExpr& set(const std::initializer_list<std::pair<std::string, FieldValue>> assigns) {
            for (auto& a : assigns) {
                assignments_.push_back(a);
            }
            return *this;
        }

        template <IsCondition Condition>
        auto where(Condition cond) const {
            return UpdateWhereExpr<Condition>{*this, std::move(cond)};
        }

        template <typename Self>
        [[nodiscard]] auto&& table(this Self&& self) {
            return std::forward<Self>(self).table_;
        }

        template <typename Self>
        [[nodiscard]] auto&& assignments(this Self&& self) {
            return std::forward<Self>(self).assignments_;
        }

    private:
        TableSchemaPtr table_{nullptr};
        std::vector<std::pair<std::string, FieldValue>> assignments_;
    };

    inline auto update(TableSchemaPtr table) {
        return UpdateExpr{std::move(table)};
    }

    inline auto update(std::string table_name) {
        return UpdateExpr{TableSchema::make_ptr(std::move(table_name))};
    }
}
