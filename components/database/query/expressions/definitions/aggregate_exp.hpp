#pragma once

#include <utility>

#include "aggregate_exp.hpp"
#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <typename Derived, typename T>
    class SingleColumnAggregateExpr : public AliasableExpression<Derived> {
    public:
        explicit SingleColumnAggregateExpr(Column<T> column)
            : column_{std::move(column)} {}

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

    protected:
        Column<T> column_;
    };

    // Count expression with optional distinct support
    template <typename T>
    class CountExpr : public AliasableExpression<CountExpr<T>> {
    public:
        explicit CountExpr(Column<T> col, const bool dist)
            : column_(std::move(col)),
              distinct_(dist) {}

        explicit CountExpr(AllColumns col, const bool dist)
            : column_(std::move(col)),
              distinct_(dist) {}

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

        [[nodiscard]] const Column<T>& column() const {
            return std::get<Column<T>>(column_);
        }

        [[nodiscard]] const AllColumns& all_columns() const {
            return std::get<AllColumns>(column_);
        }

        [[nodiscard]] bool is_all_columns() const {
            return std::holds_alternative<AllColumns>(column_);
        }

    private:
        std::variant<Column<T>, AllColumns> column_;
        bool distinct_{false};
    };

    // Simple aggregate expressions
    template <typename T>
    class SumExpr : public SingleColumnAggregateExpr<SumExpr<T>, T> {
    public:
        explicit SumExpr(Column<T> column)
            : SingleColumnAggregateExpr<SumExpr<T>, T>(std::move(column)) {}
    };

    template <typename T>
    class AvgExpr : public SingleColumnAggregateExpr<AvgExpr<T>, T> {
    public:
        constexpr explicit AvgExpr(Column<T> column)
            : SingleColumnAggregateExpr<AvgExpr<T>, T>(std::move(column)) {}
    };

    template <typename T>
    class MaxExpr : public SingleColumnAggregateExpr<MaxExpr<T>, T> {
    public:
        constexpr explicit MaxExpr(Column<T> column)
            : SingleColumnAggregateExpr<MaxExpr<T>, T>(std::move(column)) {}
    };

    template <typename T>
    class MinExpr : public SingleColumnAggregateExpr<MinExpr<T>, T> {
    public:
        constexpr explicit MinExpr(Column<T> column)
            : SingleColumnAggregateExpr<MinExpr<T>, T>(std::move(column)) {}
    };


    // Aggregate function factories
    template <typename T>
    CountExpr<T> count(const Column<T>& col) {
        return CountExpr<T>{col, false};
    }

    template <typename T>
    CountExpr<T> count(Column<T>&& col) {
        return CountExpr<T>{std::move(col), false};
    }

    template <typename T>
    CountExpr<T> count_distinct(const Column<T>& col) {
        return CountExpr<T>{col, true};
    }
    
    template <typename T>
    CountExpr<T> count_distinct(Column<T>&& col) {
        return CountExpr<T>{std::move(col), true};
    }

    inline CountExpr<void> count_all(std::string table) {
        return CountExpr<void>{AllColumns{std::move(table)}, false};
    }

    inline CountExpr<void> count_all_distinct(std::string table) {
        return CountExpr<void>{AllColumns{std::move(table)}, true};
    }
    template <typename T>
    SumExpr<T> sum(const Column<T>& col) {
        return SumExpr<T>{col};
    }

    template <typename T>
    AvgExpr<T> avg(const Column<T>& col) {
        return AvgExpr<T>{col};
    }

    template <typename T>
    MaxExpr<T> max(const Column<T>& col) {
        return MaxExpr<T>{col};
    }

    template <typename T>
    MinExpr<T> min(const Column<T>& col) {
        return MinExpr<T>{col};
    }


    template <typename T>
    SumExpr<T> sum(Column<T>&& col) {
        return SumExpr<T>{std::move(col)};
    }

    template <typename T>
    AvgExpr<T> avg(Column<T>&& col) {
        return AvgExpr<T>{std::move(col)};
    }

    template <typename T>
    MaxExpr<T> max(Column<T>&& col) {
        return MaxExpr<T>{std::move(col)};
    }

    template <typename T>
    MinExpr<T> min(Column<T>&& col) {
        return MinExpr<T>{std::move(col)};
    }
}
