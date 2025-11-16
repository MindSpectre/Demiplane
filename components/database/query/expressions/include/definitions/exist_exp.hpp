#pragma once

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class ExistsExpr : public Expression<ExistsExpr<Query>> {
    public:
        template <IsQuery SubQueryT>
        constexpr explicit ExistsExpr(SubQueryT&& sq) noexcept
            : query_{std::forward<SubQueryT>(sq)} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

    private:
        Query query_;
    };

    template <IsQuery Query>
    constexpr auto exists(Query&& query) {
        return ExistsExpr<std::remove_cvref_t<Query>>{std::forward<Query>(query)};
    }
}  // namespace demiplane::db
