#pragma once

#include <utility>

#include "db_column.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    template <typename T>
    class CountExpr : public Expression<CountExpr<T>> {
    public:
        explicit CountExpr(Column<T> col)
            : column_(std::move(col)) {}

        CountExpr(Column<T> col, const bool dist, std::optional<std::string> alias = std::nullopt)
            : column_(std::move(col)),
              distinct_(dist),
              alias_(std::move(alias)) {}

        CountExpr& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] bool distinct() const {
            return distinct_;
        }

        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        bool distinct_{false};
        std::optional<std::string> alias_;
    };


    template <typename T>
    class SumExpr : public Expression<SumExpr<T>> {
    public:
        SumExpr(Column<T> column, std::optional<std::string> alias)
            : column_{std::move(column)},
              alias_{std::move(alias)} {}

        SumExpr& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }


        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] std::optional<std::string> alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        std::optional<std::string> alias_;
    };

    template <typename T>
    class AvgExpr : public Expression<AvgExpr<T>> {
    public:
        AvgExpr(Column<T> column, std::optional<std::string> alias)
            : column_{std::move(column)},
              alias_{std::move(alias)} {}

        AvgExpr& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] std::optional<std::string> alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        std::optional<std::string> alias_;
    };

    template <typename T>
    class MaxExpr : public Expression<MaxExpr<T>> {
    public:
        MaxExpr(Column<T> column, std::optional<std::string> alias)
            : column_{std::move(column)},
              alias_{std::move(alias)} {}

        MaxExpr& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] std::optional<std::string> alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        std::optional<std::string> alias_;
    };

    template <typename T>
    class MinExpr : public Expression<MinExpr<T>> {
    public:
        MinExpr(Column<T> column, std::optional<std::string> alias)
            : column_{std::move(column)},
              alias_{std::move(alias)} {}

        MinExpr& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }

        [[nodiscard]] const Column<T>& column() const {
            return column_;
        }

        [[nodiscard]] std::optional<std::string> alias() const {
            return alias_;
        }

    private:
        Column<T> column_;
        std::optional<std::string> alias_;
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
        return SumExpr<T>{col, std::nullopt};
    }

    template <typename T>
    AvgExpr<T> avg(const Column<T>& col) {
        return AvgExpr<T>{col, std::nullopt};
    }

    template <typename T>
    MaxExpr<T> max(const Column<T>& col) {
        return MaxExpr<T>{col, std::nullopt};
    }

    template <typename T>
    MinExpr<T> min(const Column<T>& col) {
        return MinExpr<T>{col, std::nullopt};
    }
}
