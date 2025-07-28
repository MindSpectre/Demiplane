#pragma once

#include <algorithm>
#include <typeindex>

#include "query_concepts.hpp"

namespace demiplane::db {
    class QueryVisitor;

    template <class Derived>
    class Expression {
    public:
        using self_type = Derived;

        [[nodiscard]] const Derived& self() const {
            return static_cast<const Derived&>(*this);
        }

        Derived& self() {
            return static_cast<Derived&>(*this);
        }

        void accept(QueryVisitor& visitor) const;
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
    struct Literal {
        T value;

        constexpr explicit Literal(T v)
            : value(std::move(v)) {}

        void accept(QueryVisitor& visitor) const;
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
