#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class Subquery : public Expression<Subquery<Query>> {
    public:
        constexpr explicit Subquery(Query q)
            : query_(std::move(q)) {}

        Subquery& as(const char* name) {
            alias_ = name;
            return *this;
        }
        [[nodiscard]] const char* alias() const {
            return alias_;
        }
        const Query& query() const {
            return query;
        }
    private:
        Query query_;
        const char* alias_{nullptr};
    };

    template <typename Q>
    constexpr auto subquery(Q query) {
        return Subquery<Q>{std::move(query)};
    }
}
