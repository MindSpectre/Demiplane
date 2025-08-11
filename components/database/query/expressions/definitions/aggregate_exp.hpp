#pragma once

#include <utility>

#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <typename T>
    class CountExpr : public AliasableExpression<CountExpr<T>> {
    public:
        explicit CountExpr(Column<T> col)
            : column_(std::move(col)) {}

        CountExpr(Column<T> col, const bool dist)
            : column_(std::move(col)),
              distinct_(dist) {}

        template <typename Self>
        [[nodiscard]] auto&& column(this Self&& self) {
            return std::forward<Self>(self).column_;
        }

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

    private:
        Column<T> column_;
        bool distinct_{false};
    };


    template <typename T>
    class SumExpr : public AliasableExpression<SumExpr<T>> {
    public:
        SumExpr(Column<T> column)
            : column_{std::move(column)} {}

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

    private:
        Column<T> column_;
    };

    template <typename T>
    class AvgExpr : public AliasableExpression<AvgExpr<T>> {
    public:
        constexpr explicit AvgExpr(Column<T> column)
            : column_{std::move(column)} {}

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

    private:
        Column<T> column_;
    };

    template <typename T>
    class MaxExpr : public AliasableExpression<MaxExpr<T>> {
    public:
        constexpr explicit MaxExpr(Column<T> column)
            : column_{std::move(column)} {}


        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

    private:
        Column<T> column_;
    };

    template <typename T>
    class MinExpr : public AliasableExpression<MinExpr<T>> {
    public:
        constexpr explicit MinExpr(Column<T> column)
            : column_{std::move(column)} {}


        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

    private:
        Column<T> column_;
    };

    // Aggregate function factories
    template <typename T>
    CountExpr<T> count(const Column<T>& col) {
        return CountExpr<T>{col, false};
    }

    template <typename T>
    CountExpr<T> count_distinct(const Column<T>& col) {
        return CountExpr<T>{col, true};
    }

    inline CountExpr<void> count_all() {
        return CountExpr{Column<void>{nullptr}};
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
}
