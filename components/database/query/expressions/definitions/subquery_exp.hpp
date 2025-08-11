#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class Subquery : public AliasableExpression<Subquery<Query>> {
    public:
        constexpr explicit Subquery(Query q)
            : query_(std::move(q)) {}

        const Query& query() const {
            return query_;
        }
    private:
        Query query_;
    };

    template <IsQuery Q>
    constexpr auto subquery(Q query) {
        return Subquery<Q>{std::move(query)};
    }
}
