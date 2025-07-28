#pragma once
#include <concepts>

#include <gears_templates.hpp>

namespace demiplane::db {
    struct OpBase;

    template <typename T>
    concept IsOperator = std::is_base_of_v<OpBase, T>;

    template <class Derived>
    class Expression;
    template <typename T>
    concept IsQuery = requires {
        // Must inherit from Expression (CRTP pattern)
        requires std::derived_from<T, Expression<T>>;
    };

    template <typename Left, typename Right, IsOperator Op>
    class BinaryExpr;

    template <typename T>
    concept IsBinaryOperand = gears::derived_from_specialization_of_v<T, BinaryExpr>;

    template <IsBinaryOperand Operand, IsOperator Op>
    class UnaryExpr;

    template <typename T>
    concept IsUnaryOperand = gears::derived_from_specialization_of_v<T, UnaryExpr>;


    template <IsQuery Query>
    class ExistsExpr;

    template <typename T>
    concept IsExistExpr = gears::derived_from_specialization_of_v<T, ExistsExpr>;

    template <typename T>
    concept IsCondition = IsBinaryOperand<T> || IsUnaryOperand<T> || IsExistExpr<T>;

    template <typename T>
    concept IsScalar = gears::always_true_v<T>; // TODO


    template <typename T>
    class CountExpr;

    template <typename T>
    class SumExpr;

    template <typename T>
    class AvgExpr;

    template <typename T>
    class MinExpr;

    template <typename T>
    class MaxExpr;


    template <typename T>
    concept IsAggregate = demiplane::gears::is_specialization_of_v<CountExpr, T> ||
                          demiplane::gears::is_specialization_of_v<SumExpr, T> ||
                          demiplane::gears::is_specialization_of_v<AvgExpr, T> ||
                          demiplane::gears::is_specialization_of_v<MaxExpr, T> ||
                          demiplane::gears::is_specialization_of_v<MinExpr, T>;

    template <typename T>
    concept IsSelectExpression = IsColumn<T> || IsAggregate<T> || IsScalar<T>;

    template <typename T>
    concept IsWhereExpression = IsColumn<T> || IsScalar<T>; // No aggregates!

    template <typename T>
    concept IsHavingExpression = IsAggregate<T> || IsScalar<T>; // Prefer aggregates

    template <typename T>
    concept IsOrderByExpression = IsColumn<T> || IsAggregate<T> || IsScalar<T>; // Context-dependent
}
