#pragma once

#include <algorithm>
#include <utility>

#include <db_core_objects.hpp>
#include <gears_concepts.hpp>

#include "db_expressions_fwd.hpp"

namespace demiplane::db {
    template <class Derived>
    class Expression {
    public:
        using self_type = Derived;
        void accept(this auto&& self, QueryVisitor& visitor);

    protected:
        constexpr decltype(auto) self(this auto&& self) {
            using Self = decltype(self);
            using D    = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const Derived, Derived>;
            return static_cast<std::conditional_t<std::is_lvalue_reference_v<Self>, D&, D&&>>(self);
        }
    };

    // Binary operators
    struct OpBase {};

    struct OpEqual : OpBase {};

    struct OpNotEqual : OpBase {};

    struct OpLess : OpBase {};

    struct OpLessEqual : OpBase {};

    struct OpGreater : OpBase {};

    struct OpGreaterEqual : OpBase {};

    struct OpAnd : OpBase {};

    struct OpOr : OpBase {};

    struct OpLike : OpBase {};

    struct OpNotLike : OpBase {};

    struct OpIn : OpBase {};

    struct OpNotIn : OpBase {};

    // Unary operators
    struct OpNot : OpBase {};

    struct OpIsNull : OpBase {};

    struct OpIsNotNull : OpBase {};

    // Join types
    enum class JoinType { INNER, LEFT, RIGHT, FULL, CROSS };

    // Set operations
    enum class SetOperation { UNION, UNION_ALL, INTERSECT, EXCEPT };

    template <typename T>
    class Literal {
    public:
        constexpr auto&& value(this auto&& self) {
            return std::forward<decltype(self)>(self).value_;
        }

        [[nodiscard]] constexpr std::string_view alias() const noexcept {
            return alias_;
        }

        template <typename ValueTp>
            requires std::constructible_from<T, ValueTp>
        constexpr explicit Literal(ValueTp&& v)
            : value_{std::forward<ValueTp>(v)} {
        }

        template <typename Self, typename Tp>
            requires std::convertible_to<Tp, std::string_view>
        constexpr auto&& as(this Self&& self, Tp&& alias) {
            self.alias_ = std::string_view{std::forward<Tp>(alias)};
            return std::forward<Self>(self);
        }

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        T value_;
        std::string_view alias_;
    };

    // Deduction guide: Literal(x) deduces T from argument
    template <typename T>
    Literal(T) -> Literal<T>;

    template <typename T>
    constexpr Literal<std::remove_cvref_t<T>> lit(T&& value) {
        return Literal<std::remove_cvref_t<T>>{std::forward<T>(value)};
    }

    constexpr Literal<std::string_view> lit(const char* value) {
        return Literal<std::string_view>{std::string_view{value}};
    }

    namespace detail {
        template <typename T>
        constexpr auto make_literal_if_needed(T&& value) {
            if constexpr (HasAcceptVisitor<T>) {
                return std::forward<T>(value);
            } else if constexpr (std::is_convertible_v<T, const char*>) {
                return Literal<std::string_view>{std::string_view{value}};
            } else {
                return Literal<std::remove_cvref_t<T>>{std::forward<T>(value)};
            }
        }

    }  // namespace detail

    // Null literal
    struct NullLiteral {};

    constexpr NullLiteral null_value{};


    template <typename Derived>
    class AliasableExpression : public Expression<Derived> {
    public:
        template <typename Tp>
            requires std::convertible_to<Tp, std::string_view>
        constexpr explicit AliasableExpression(Tp&& alias)
            : alias_{std::string_view{std::forward<Tp>(alias)}} {
        }

        template <typename Self, typename T>
            requires std::convertible_to<T, std::string_view>
        constexpr auto&& as(this Self&& self, T&& name) {
            self.alias_ = std::string_view{std::forward<T>(name)};
            return static_cast<std::conditional_t<std::is_lvalue_reference_v<Self>, Derived&, Derived&&>>(
                std::forward<Self>(self));
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& alias(this Self&& self) noexcept {
            return std::forward<Self>(self).alias_;
        }

    protected:
        std::string_view alias_;
        constexpr AliasableExpression() = default;
    };

    struct AllowWhere {};

    struct AllowGroupBy {};

    struct AllowHaving {};

    struct AllowJoin {};

    struct AllowOrderBy {};

    struct AllowLimit {};

    struct AllowDistinct {};

    struct AllowUnion {};


    template <typename Feature, typename... Features>
    constexpr bool has_feature = (std::is_same_v<Feature, Features> || ...);

    template <typename Parent>
    class JoinBuilder {
    public:
        template <typename P, typename T>
            requires std::same_as<std::remove_cvref_t<P>, Parent> && std::constructible_from<TablePtr, T>
        constexpr JoinBuilder(P&& parent, T&& right_table, const JoinType type)
            : parent_{std::forward<P>(parent)},
              right_table_name_{std::forward<T>(right_table)},
              type_{type} {
        }

        template <typename Self, typename Tp>
            requires std::convertible_to<Tp, std::string_view>
        constexpr auto&& as(this Self&& self, Tp&& name) {
            self.right_alias_ = std::string_view{std::forward<Tp>(name)};
            return std::forward<Self>(self);
        }

        template <typename Self, IsCondition Condition>
        constexpr auto on(this Self&& self, Condition&& cond) {
            return JoinExpr<Parent, std::remove_cvref_t<Condition>>{std::forward<Self>(self).parent_,
                                                                    std::forward<Self>(self).right_table_name_,
                                                                    std::forward<Condition>(cond),
                                                                    std::forward<Self>(self).type_,
                                                                    std::forward<Self>(self).right_alias_};
        }

    private:
        Parent parent_;
        TablePtr right_table_name_;
        JoinType type_;
        std::string_view right_alias_;
    };

    class ColumnHolder {
    public:
        // Accept any DynamicColumn<S> — converts to owning DynamicColumn<std::string>
        template <typename DynamicColumnTp>
            requires IsDynamicColumn<DynamicColumnTp>
        constexpr explicit ColumnHolder(DynamicColumnTp&& column)
            : column_{DynamicColumn<std::string>{std::string{column.name()},
                                                  std::string{column.context()}}} {
        }

        template <typename AllColumnsTp>
            requires std::is_same_v<std::remove_cvref_t<AllColumnsTp>, AllColumns>
        constexpr explicit ColumnHolder(AllColumnsTp&& column) noexcept
            : column_{std::forward<AllColumnsTp>(column)} {
        }

        template <typename T>
            requires std::constructible_from<AllColumns, T> && (!std::same_as<std::remove_cvref_t<T>, AllColumns>)
        constexpr explicit ColumnHolder(T&& column)
            : column_{AllColumns{std::forward<T>(column)}} {
        }

        [[nodiscard]] constexpr const DynamicColumn<std::string>& column() const& {
            return std::get<DynamicColumn<std::string>>(column_);
        }

        [[nodiscard]] constexpr const AllColumns& all_columns() const& {
            return std::get<AllColumns>(column_);
        }

        [[nodiscard]] constexpr bool is_all_columns() const& {
            return std::holds_alternative<AllColumns>(column_);
        }

    private:
        std::variant<DynamicColumn<std::string>, AllColumns> column_;
    };

    template <IsTable TableT>
    class TableHolder {
    public:
        template <typename TableTp>
            requires(!std::same_as<std::remove_cvref_t<TableTp>, TableHolder>) &&
                    std::constructible_from<TableT, TableTp>
        constexpr explicit TableHolder(TableTp&& table) noexcept
            : table_{std::forward<TableTp>(table)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& table(this Self&& self) noexcept {
            return std::forward<Self>(self).table_;
        }

    private:
        TableT table_;
    };

    template <typename Derived, typename... AllowedFeatures>
    class QueryOperations {
    public:
        // WHERE
        template <typename Self, IsCondition Condition>
            requires(has_feature<AllowWhere, AllowedFeatures...>)
        [[nodiscard]] constexpr auto where(this Self&& self, Condition&& cond) {
            return WhereExpr<Derived, std::remove_cvref_t<Condition>>{std::forward<Self>(self).derived(),
                                                                      std::forward<Condition>(cond)};
        }

        // GROUP BY
        template <typename Self, IsColumn... GroupColumns>
            requires(has_feature<AllowGroupBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto group_by(this Self&& self, GroupColumns&&... cols) {
            return GroupByColumnExpr<Derived, std::remove_cvref_t<GroupColumns>...>{
                std::forward<Self>(self).derived(), std::forward<GroupColumns>(cols)...};
        }

        template <typename Self, IsQuery GroupingCriteria>
            requires(has_feature<AllowGroupBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto group_by(this Self&& self, GroupingCriteria&& query) {
            return GroupByQueryExpr<Derived, std::remove_cvref_t<GroupingCriteria>>{
                std::forward<Self>(self).derived(), std::forward<GroupingCriteria>(query)};
        }

        // HAVING
        template <typename Self, IsCondition Condition>
            requires(has_feature<AllowHaving, AllowedFeatures...>)
        [[nodiscard]] constexpr auto having(this Self&& self, Condition&& cond) {
            return HavingExpr<Derived, std::remove_cvref_t<Condition>>{std::forward<Self>(self).derived(),
                                                                       std::forward<Condition>(cond)};
        }

        // JOIN
        template <typename Self, typename TableType>
            requires(has_feature<AllowJoin, AllowedFeatures...>)
        [[nodiscard]] constexpr auto join(this Self&& self, TableType&& table, JoinType type = JoinType::INNER) {
            if constexpr (std::is_same_v<std::decay_t<TableType>, std::string>) {
                return JoinBuilder<Derived>{
                    std::forward<Self>(self).derived(), Table::make_ptr(std::forward<TableType>(table)), type};
            } else {
                return JoinBuilder<Derived>{std::forward<Self>(self).derived(), std::forward<TableType>(table), type};
            }
        }

        // ORDER BY
        template <typename Self, IsOrderBy... Orders>
            requires(has_feature<AllowOrderBy, AllowedFeatures...>)
        [[nodiscard]] constexpr auto order_by(this Self&& self, Orders&&... orders) {
            return OrderByExpr<Derived, std::remove_cvref_t<Orders>...>{std::forward<Self>(self).derived(),
                                                                        std::forward<Orders>(orders)...};
        }

        // LIMIT
        template <typename Self, typename T = void>
            requires(has_feature<AllowLimit, AllowedFeatures...>)
        [[nodiscard]] constexpr auto limit(this Self&& self, std::size_t count) {
            return LimitExpr<Derived>{std::forward<Self>(self).derived(), count, 0};
        }

    protected:
        // Helper to get derived instance
        [[nodiscard]] constexpr auto&& derived(this auto&& self) {
            return static_cast<std::conditional_t<
                std::is_const_v<std::remove_reference_t<decltype(self)>>,
                std::conditional_t<std::is_lvalue_reference_v<decltype(self)>, const Derived&, const Derived&&>,
                std::conditional_t<std::is_lvalue_reference_v<decltype(self)>, Derived&, Derived&&>>>(
                std::forward<decltype(self)>(self));
        }
    };
}  // namespace demiplane::db
