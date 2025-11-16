#pragma once

#include <string>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class CteExpr : public Expression<CteExpr<Query>> {
    public:
        template <typename StringTp = std::string, typename QueryTp = std::remove_cvref_t<Query>>
        constexpr CteExpr(StringTp&& name, QueryTp&& q, const bool recursive = false) noexcept
            : cte_name_{std::forward<StringTp>(name)},
              query_{std::forward<QueryTp>(q)},
              recursive_{recursive} {
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& name(this Self&& self) noexcept {
            return std::forward<Self>(self).cte_name_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& query(this Self&& self) noexcept {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] constexpr bool recursive() const noexcept {
            return recursive_;
        }

    private:
        std::string cte_name_;
        Query query_;
        bool recursive_ = false;
    };

    template <IsQuery Query, typename StringTp = std::string>
    constexpr auto with(StringTp&& name, Query&& query) {
        return CteExpr<std::remove_cvref_t<Query>>{std::forward<StringTp>(name), std::forward<Query>(query), false};
    }

    template <IsQuery Query, typename StringTp = std::string>
    constexpr auto with_recursive(StringTp&& name, Query&& query) {
        return CteExpr<std::remove_cvref_t<Query>>{std::forward<StringTp>(name), std::forward<Query>(query), true};
    }
}  // namespace demiplane::db
