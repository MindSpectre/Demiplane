#pragma once
namespace demiplane::db {
    template <typename T>
    void Column<T>::accept(QueryVisitor& visitor) const {
        visitor.visit(*this);
    }

    // Same for Column<void> specialization
    inline void Column<void>::accept(QueryVisitor& visitor) const {
        visitor.visit(*this);
    }

    template <typename Derived>
    void Expression<Derived>::accept(QueryVisitor& visitor) const {
        visitor.visit(self());
    }

    template <typename T>
    void Literal<T>::accept(QueryVisitor& visitor) const {
        visitor.visit(*this);
    }
}
