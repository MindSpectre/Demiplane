#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class LimitExpr : public Expression<LimitExpr<Query>> {
    public:
        constexpr LimitExpr(Query query, const std::size_t limit, const std::size_t offset)
            : query_(std::move(query)),
              count_(limit),
              offset_(offset) {}

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

        template <typename Self>
        [[nodiscard]] auto&& offset(this Self&& self, const std::size_t offset) {
            return LimitExpr{std::forward<Self>(self).query_, std::forward<Self>(self).count_, offset};
        }

    private:
        Query query_;
        std::size_t count_;
        std::size_t offset_;
    };
}