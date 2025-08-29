#pragma once

#include <algorithm>
#include <string>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class CteExpr : public Expression<CteExpr<Query>> {
        public:
        CteExpr(std::string name, Query q, const bool r = false)
            : cte_name_(std::move(name)),
              query_(std::move(q)),
              recursive_(r) {
        }


        [[nodiscard]] const std::string& name() const {
            return cte_name_;
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] bool recursive() const {
            return recursive_;
        }

        private:
        std::string cte_name_;
        Query query_;
        bool recursive_{false};
    };

    template <IsQuery Query>
    auto with(std::string name, Query query) {
        return CteExpr<Query>{std::move(name), std::move(query), false};
    }

    template <IsQuery Query>
    auto with_recursive(std::string name, Query query) {
        return CteExpr<Query>{std::move(name), std::move(query), true};
    }
}  // namespace demiplane::db
