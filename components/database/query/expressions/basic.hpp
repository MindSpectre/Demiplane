#pragma once

#include <algorithm>
#include <typeindex>

#include "db_expressions_fwd.hpp"

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

    template <typename Derived>
    class AliasableExpression : public Expression<Derived> {
    public:
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

        auto&& alias(this auto&& self) {
            return std::forward<decltype(self)>(self).alias_;
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

    // Parameter placeholder for prepared statements
    struct Parameter {
        std::size_t index;
        std::type_index type;

        Parameter(const std::size_t idx, const std::type_index t)
            : index(idx),
              type(t) {}
    };
}
