#pragma once

#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <typename T>
    class CountExpr : public Expression<CountExpr<T>> {
    public:
        explicit CountExpr(Column<T> col)
            : column_(std::move(col)) {}

        CountExpr(Column<T> col, const bool dist)
            : column_(std::move(col)),
              distinct_(dist) {}

        CountExpr(Column<T> col, const bool dist, const char* a)
            : column_(std::move(col)),
              distinct_(dist),
              alias_(a) {}

        CountExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        bool distinct_{false};
        const char* alias_{nullptr};
    };


    template <typename T>
    class SumExpr : public Expression<SumExpr<T>> {
    public:
        SumExpr(Column<T> column, const char* alias)
            : column_{std::move(column)},
              alias_{alias} {}
        SumExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }


        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        const char* alias_{nullptr};
    };

    template <typename T>
    class AvgExpr : public Expression<AvgExpr<T>> {
    public:

        AvgExpr(Column<T> column, const char* const alias)
            : column_{std::move(column)},
              alias_{alias} {}
        AvgExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        const char* alias_{nullptr};
    };

    template <typename T>
    class MaxExpr : public Expression<MaxExpr<T>> {
    public:
        MaxExpr(Column<T> column, const char* const alias)
            : column_{std::move(column)},
              alias_{alias} {}

        MaxExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        const char* alias_{nullptr};
    };

    template <typename T>
    class MinExpr : public Expression<MinExpr<T>> {
    public:
        MinExpr(Column<T> column, const char* const alias)
            : column_{std::move(column)},
              alias_{alias} {}
        MinExpr& as(const char* name) {
            alias_ = name;
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] const char* alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        const char* alias_{nullptr};
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
        return CountExpr{Column<void>{nullptr, nullptr}};
    }


    template <typename T>
    SumExpr<T> sum(const Column<T>& col) {
        return SumExpr<T>{col, nullptr};
    }

    template <typename T>
    AvgExpr<T> avg(const Column<T>& col) {
        return AvgExpr<T>{col, nullptr};
    }

    template <typename T>
    MaxExpr<T> max(const Column<T>& col) {
        return MaxExpr<T>{col, nullptr};
    }

    template <typename T>
    MinExpr<T> min(const Column<T>& col) {
        return MinExpr<T>{col, nullptr};
    }
}
