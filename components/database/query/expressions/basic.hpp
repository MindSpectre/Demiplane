#pragma once

#include <algorithm>
#include <utility>

#include "db_expressions_fwd.hpp"
#include "db_table_schema.hpp"

namespace demiplane::db {
    template <class Derived>
    class Expression {
    public:
        using self_type = Derived;
        void accept(this auto&& self, QueryVisitor& visitor);

    protected:
        auto&& self(this auto&& self) {
            return static_cast<std::conditional_t<
                std::is_const_v<std::remove_reference_t<decltype(self)>>,
                const Derived,
                Derived
            >&&>(self);
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
    enum class JoinType {
        INNER,
        LEFT,
        RIGHT,
        FULL,
        CROSS
    };

    // Set operations
    enum class SetOperation {
        UNION,
        UNION_ALL,
        INTERSECT,
        EXCEPT
    };

    template <typename T>
    class Literal {
    public:
        auto&& value(this auto&& self) {
            return std::forward<decltype(self)>(self).value_;
        }

        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }

        constexpr explicit Literal(T v)
            : value_(std::move(v)) {}

        Literal& as(std::optional<std::string> alias) {
            alias_ = std::move(alias);
            return *this;
        }

        void accept(this auto&& self, QueryVisitor& visitor);

    private:
        T value_;
        std::optional<std::string> alias_;
    };

    template <typename T>
    constexpr Literal<T> lit(T value) {
        return Literal<T>{std::move(value)};
    }

    // Null literal
    struct NullLiteral {};

    constexpr NullLiteral null_value{};


    template <typename Derived>
    class AliasableExpression : public Expression<Derived> {
    public:
        constexpr explicit AliasableExpression(std::optional<std::string> alias)
            : alias_(std::move(alias)) {}

        Derived& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return static_cast<Derived&>(*this);
        }

        template <typename Self>
        [[nodiscard]] auto&& alias(this Self&& self) {
            return std::forward<Self>(self).alias_;
        }

    protected:
        std::optional<std::string> alias_;
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
        JoinBuilder(Parent parent, TableSchemaPtr right_table, JoinType type)
            : parent_{std::move(parent)},
              right_table_name_{std::move(right_table)},
              type_{type} {}

        JoinBuilder& as(std::optional<std::string> name) {
            right_alias_ = std::move(name);
            return *this;
        }

        template <IsCondition Condition>
        auto on(Condition cond) && {
            return JoinExpr<Parent, Condition>{
                std::move(parent_),
                std::move(right_table_name_),
                std::move(cond),
                type_,
                right_alias_
            };
        }

        template <IsCondition Condition>
        auto on(Condition cond) const & {
            return JoinExpr<Parent, Condition>{
                parent_,
                right_table_name_,
                std::move(cond),
                type_,
                right_alias_
            };
        }

    private:
        Parent parent_;
        TableSchemaPtr right_table_name_;
        JoinType type_;
        std::optional<std::string> right_alias_;
    };

    class ColumnHolder {
    public:
        explicit ColumnHolder(DynamicColumn column)
            : column_{std::move(column)} {}

        explicit ColumnHolder(AllColumns column)
            : column_{std::move(column)} {}

        [[nodiscard]] const DynamicColumn& column() const {
            return std::get<DynamicColumn>(column_);
        }

        [[nodiscard]] const AllColumns& all_columns() const {
            return std::get<AllColumns>(column_);
        }

        [[nodiscard]] constexpr bool is_all_columns() const {
            return std::holds_alternative<AllColumns>(column_);
        }

    private:
        std::variant<DynamicColumn, AllColumns> column_;
    };


    template <typename Derived, typename... AllowedFeatures>
    class QueryOperations {
    public:
        // WHERE - only if AllowWhere is present
        template <IsCondition Condition>
        auto where(Condition cond) const &
            requires (has_feature<AllowWhere, AllowedFeatures...>) {
            return WhereExpr<Derived, Condition>{derived(), std::move(cond)};
        }

        template <IsCondition Condition>
        auto where(Condition cond) &&
            requires (has_feature<AllowWhere, AllowedFeatures...>) {
            return WhereExpr<Derived, Condition>{std::move(derived()), std::move(cond)};
        }

        // GROUP BY - only if AllowGroupBy is present
        template <IsColumn... GroupColumns>
        auto group_by(GroupColumns... cols) const &
            requires (has_feature<AllowGroupBy, AllowedFeatures...>) {
            return GroupByColumnExpr<Derived, GroupColumns...>{derived(), cols...};
        }

        template <IsColumn... GroupColumns>
        auto group_by(GroupColumns... cols) &&
            requires (has_feature<AllowGroupBy, AllowedFeatures...>) {
            return GroupByColumnExpr<Derived, GroupColumns...>{std::move(derived()), cols...};
        }

        template <IsQuery GroupingCriteria>
        auto group_by(GroupingCriteria&& query) const &
            requires (has_feature<AllowGroupBy, AllowedFeatures...>) {
            return GroupByQueryExpr<Derived, GroupingCriteria>{
                derived(),
                std::forward<GroupingCriteria>(query)
            };
        }

        template <IsQuery GroupingCriteria>
        auto group_by(GroupingCriteria&& query) &&
            requires (has_feature<AllowGroupBy, AllowedFeatures...>) {
            return GroupByQueryExpr<Derived, GroupingCriteria>{
                std::move(derived()),
                std::forward<GroupingCriteria>(query)
            };
        }

        // HAVING - only if AllowHaving is present
        template <IsCondition Condition>
        auto having(Condition cond) const &
            requires (has_feature<AllowHaving, AllowedFeatures...>) {
            return HavingExpr<Derived, Condition>{derived(), std::move(cond)};
        }

        template <IsCondition Condition>
        auto having(Condition cond) &&
            requires (has_feature<AllowHaving, AllowedFeatures...>) {
            return HavingExpr<Derived, Condition>{std::move(derived()), std::move(cond)};
        }

        // JOIN - only if AllowJoin is present
        auto join(std::string table, JoinType type = JoinType::INNER) const &
            requires (has_feature<AllowJoin, AllowedFeatures...>) {
            return JoinBuilder<Derived>{derived(), TableSchema::make_ptr(std::move(table)), type};
        }

        auto join(TableSchemaPtr table, JoinType type = JoinType::INNER) const &
            requires (has_feature<AllowJoin, AllowedFeatures...>) {
            return JoinBuilder<Derived>{derived(), table, type};
        }

        auto join(std::string table, JoinType type = JoinType::INNER) &&
            requires (has_feature<AllowJoin, AllowedFeatures...>) {
            return JoinBuilder<Derived>{std::move(derived()), TableSchema::make_ptr(std::move(table)), type};
        }

        auto join(TableSchemaPtr table, JoinType type = JoinType::INNER) &&
            requires (has_feature<AllowJoin, AllowedFeatures...>) {
            return JoinBuilder<Derived>{std::move(derived()), std::move(table), type};
        }

        // ORDER BY - only if AllowOrderBy is present
        template <IsOrderBy... Orders>
        auto order_by(Orders... orders) const &
            requires (has_feature<AllowOrderBy, AllowedFeatures...>) {
            return OrderByExpr<Derived, Orders...>{derived(), orders...};
        }

        template <IsOrderBy... Orders>
        auto order_by(Orders... orders) &&
            requires (has_feature<AllowOrderBy, AllowedFeatures...>) {
            return OrderByExpr<Derived, Orders...>{std::move(derived()), orders...};
        }

        // LIMIT - only if AllowLimit is present
        auto limit(std::size_t count) const &
            requires (has_feature<AllowLimit, AllowedFeatures...>) {
            return LimitExpr<Derived>{derived(), count, 0};
        }

        auto limit(std::size_t count) &&
            requires (has_feature<AllowLimit, AllowedFeatures...>) {
            return LimitExpr<Derived>{std::move(derived()), count, 0};
        }

        // TODO: make chaining set operation(union, intersect, except, union_all) Issue#41

    protected:
        // Helper to get derived instance
        const Derived& derived() const & {
            return static_cast<const Derived&>(*this);
        }

        Derived&& derived() && {
            return static_cast<Derived&&>(*this);
        }

        Derived& derived() & {
            return static_cast<Derived&>(*this);
        }
    };
}
