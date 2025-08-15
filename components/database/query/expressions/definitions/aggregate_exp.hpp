#pragma once

#include <utility>

#include "aggregate_exp.hpp"
#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <typename Derived, typename T>
    class SingleColumnAggregateExpr : public AliasableExpression<Derived> {
    public:
        explicit SingleColumnAggregateExpr(TableColumn<T> column)
            : column_{std::move(column)} {}

        [[nodiscard]] const TableColumn<T>& column() const {
            return column_;
        }

    protected:
        TableColumn<T> column_;
    };

    // Count expression with optional distinct support
    class CountExpr : public AliasableExpression<CountExpr>, public ColumnHolder {
    public:
        explicit CountExpr(DynamicColumn col, const bool dist)
            : ColumnHolder{std::move(col)},
              distinct_(dist) {}

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

    private:
        bool distinct_{false};
    };

    // Simple aggregate expressions
    class SumExpr : public AliasableExpression<SumExpr>, public ColumnHolder {
    public:
        explicit SumExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {}
    };

    class AvgExpr : public AliasableExpression<AvgExpr>, public ColumnHolder {
    public:
        explicit AvgExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {}
    };

    class MaxExpr : public AliasableExpression<MaxExpr>, public ColumnHolder {
    public:
        explicit MaxExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {}
    };


    class MinExpr : public AliasableExpression<MinExpr>, public ColumnHolder {
    public:
        explicit MinExpr(DynamicColumn column)
            : ColumnHolder(std::move(column)) {}
    };


    // Aggregate function factories
    template <typename T>
    CountExpr count(const TableColumn<T>& col) {
        return CountExpr{col.as_dynamic(), false};
    }


    template <typename T>
    CountExpr count_distinct(const TableColumn<T>& col) {
        return CountExpr{col.as_dynamic(), true};
    }

    //todo: Count(table.*) will work?
    inline CountExpr count_all(std::string table) {
        return CountExpr{AllColumns{std::move(table)}.as_dynamic().set_context({}), false};
    }

    inline CountExpr count_all_distinct(std::string table) {
        return CountExpr{AllColumns{std::move(table)}.as_dynamic(), true};
    }

    template <typename T>
    SumExpr sum(const TableColumn<T>& col) {
        return SumExpr{col.as_dynamic()};
    }

    template <typename T>
    AvgExpr avg(const TableColumn<T>& col) {
        return AvgExpr{col.as_dynamic()};
    }

    template <typename T>
    MaxExpr max(const TableColumn<T>& col) {
        return MaxExpr{col.as_dynamic()};
    }

    template <typename T>
    MinExpr min(const TableColumn<T>& col) {
        return MinExpr{col.as_dynamic()};
    }

}
