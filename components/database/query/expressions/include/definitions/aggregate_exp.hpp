#pragma once

#include <utility>

#include "../basic.hpp"
#include "aggregate_exp.hpp"

namespace demiplane::db {
    // Count expression with optional distinct support
    class CountExpr : public AliasableExpression<CountExpr>, public ColumnHolder {
    public:
        constexpr CountExpr(DynamicColumn col, const bool dist)
            : ColumnHolder{std::move(col)},
              distinct_(dist) {
        }

        constexpr CountExpr(AllColumns col, const bool dist)
            : ColumnHolder{std::move(col)},
              distinct_{dist} {
        }

        [[nodiscard]] constexpr bool distinct() const {
            return distinct_;
        }

    private:
        bool distinct_ = false;
    };

    // Simple aggregate expressions
    class SumExpr : public AliasableExpression<SumExpr>, public ColumnHolder {
    public:
        constexpr explicit SumExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {
        }
    };

    class AvgExpr : public AliasableExpression<AvgExpr>, public ColumnHolder {
    public:
        constexpr explicit AvgExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {
        }
    };

    class MaxExpr : public AliasableExpression<MaxExpr>, public ColumnHolder {
    public:
        constexpr explicit MaxExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {
        }
    };


    class MinExpr : public AliasableExpression<MinExpr>, public ColumnHolder {
    public:
        constexpr explicit MinExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {
        }
    };


    // Aggregate function factories
    template <typename T>
    constexpr CountExpr count(const TableColumn<T>& col) {
        return CountExpr{col.as_dynamic(), false};
    }


    template <typename T>
    constexpr CountExpr count_distinct(const TableColumn<T>& col) {
        return CountExpr{col.as_dynamic(), true};
    }

    inline CountExpr count_all() {
        return CountExpr{AllColumns{""}, false};
    }

    inline CountExpr count_all_distinct() {
        return CountExpr{AllColumns{""}, true};
    }

    template <typename T>
    constexpr SumExpr sum(const TableColumn<T>& col) {
        return SumExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr AvgExpr avg(const TableColumn<T>& col) {
        return AvgExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr MaxExpr max(const TableColumn<T>& col) {
        return MaxExpr{col.as_dynamic()};
    }

    template <typename T>
    constexpr MinExpr min(const TableColumn<T>& col) {
        return MinExpr{col.as_dynamic()};
    }
}  // namespace demiplane::db
