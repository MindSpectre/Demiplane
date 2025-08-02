#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class Subquery : public Expression<Subquery<Query>> {
    public:
        constexpr explicit Subquery(Query q)
            : query_(std::move(q)) {}

        Subquery& as(std::optional<std::string> name) {
            alias_ = std::move(name);
            return *this;
        }

        [[nodiscard]] const std::optional<std::string>& alias() const {
            return alias_;
        }
        const Query& query() const {
            return query;
        }
    private:
        Query query_;
        std::optional<std::string> alias_;
    };

    template <typename Q>
    constexpr auto subquery(Q query) {
        return Subquery<Q>{std::move(query)};
    }
}
