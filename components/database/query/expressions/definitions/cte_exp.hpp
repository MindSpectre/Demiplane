#pragma once

#include <algorithm>
#include <string>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsQuery Query>
    class CteExpr : public Expression<CteExpr<Query>> {
    public:
        CteExpr(std::string n, Query q, const bool r = false)
            : name_(std::move(n)),
              query_(std::move(q)),
              recursive_(r) {}

        template <typename Self>
        [[nodiscard]] auto&& name(this Self&& self) {
            return std::forward<Self>(self).name_;
        }

        template <typename Self>
        [[nodiscard]] auto&& query(this Self&& self) {
            return std::forward<Self>(self).query_;
        }

        [[nodiscard]] bool recursive() const {
            return recursive_;
        }

    private:
        std::string name_;
        Query query_;
        bool recursive_{false};
    };

    template <IsQuery Query>
    auto with(const std::string& name, Query query) {
        return CteExpr<Query>{name, std::move(query), false};
    }

    template <IsQuery Query>
    auto with_recursive(const std::string& name, Query query) {
        return CteExpr<Query>{name, std::move(query), true};
    }
}
