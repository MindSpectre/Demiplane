#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
class LimitExpr : public Expression<LimitExpr<Query>> {
public:
    constexpr LimitExpr(Query q, std::size_t c, std::size_t o)
            : query_(std::move(q)),
              count_(c),
              offset_(o) {}

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] std::size_t count() const {
            return count_;
        }

        [[nodiscard]] std::size_t offset() const {
            return offset_;
        }

    private:
        Query query_;
        std::size_t count_;
        std::size_t offset_;
    };
}
