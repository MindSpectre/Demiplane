#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class ExistsExpr : public Expression<ExistsExpr<Query>> {
    public:
        constexpr explicit ExistsExpr(Query sq)
            : query_(std::move(sq)) {}

        [[nodiscard]] const Query& query() const {
            return query_;
        }

    private:
        Query query_;
    };

    template <IsQuery Query>
    constexpr auto exists(Query query) {
        return ExistsExpr<Query>{std::move(query)};
    }
}
