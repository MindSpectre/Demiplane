#pragma once

#include <algorithm>
#include <string>

namespace demiplane::db {
    template <IsQuery Query>
    class CteExpr : public Expression<CteExpr<Query>> {
    public:
        CteExpr(std::string n, Query q, const bool r = false)
            : name_(std::move(n)),
              query_(std::move(q)),
              recursive_(r) {}

        [[nodiscard]] const std::string& name() const {
            return name_;
        }

        [[nodiscard]] const Query& query() const {
            return query_;
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
