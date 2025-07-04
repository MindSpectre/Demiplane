#pragma once

#include <string>
namespace demiplane::database::query {

    // Миксин для добавления условий с использованием SIMILAR TO.
    template <typename Derived>
    class SimilarityConditionContext {
    public:
        /**
         * Add condition for similarity check(FTS,Trgm, Like)
         *
         * @param pattern what we look for
         */
        Derived& similar(std::string pattern) {
            pattern_ = std::move(pattern);
            return static_cast<Derived&>(*this);
        }
        [[nodiscard]] std::string_view pattern() const {
            return pattern_;
        }
        [[nodiscard]] bool has_pattern() const {
            return !pattern_.empty();
        }
    protected:
        std::string pattern_;
    };
} // namespace demiplane::database::query
