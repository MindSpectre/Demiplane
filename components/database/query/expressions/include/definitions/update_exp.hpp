#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    class UpdateExpr : public Expression<UpdateExpr> {
    public:
        constexpr explicit UpdateExpr(TablePtr t)
            : table_(std::move(t)) {
        }

        constexpr UpdateExpr& set(std::string column, FieldValue value) {
            assignments_.emplace_back(std::move(column), std::move(value));
            return *this;
        }

        constexpr UpdateExpr& set(const std::initializer_list<std::pair<std::string, FieldValue>> assigns) {
            for (auto& a : assigns) {
                assignments_.push_back(a);
            }
            return *this;
        }

        template <IsCondition Condition>
        constexpr auto where(Condition cond) const {
            return UpdateWhereExpr<Condition>{*this, std::move(cond)};
        }

        template <typename Self>
        [[nodiscard]]constexpr  auto&& table(this Self&& self) {
            return std::forward<Self>(self).table_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& assignments(this Self&& self) {
            return std::forward<Self>(self).assignments_;
        }

    private:
        TablePtr table_ = nullptr;
        std::vector<std::pair<std::string, FieldValue>> assignments_;
    };

    inline auto update(TablePtr table) {
        return UpdateExpr{std::move(table)};
    }

    inline auto update(std::string table_name) {
        return UpdateExpr{Table::make_ptr(std::move(table_name))};
    }
}  // namespace demiplane::db
