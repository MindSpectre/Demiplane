#pragma once

namespace demiplane::db {
    template <typename T>
    void Column<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
    // Same for Column<void> specialization
    void Column<void>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename Derived>
    void Expression<Derived>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self).self());
    }

    template <typename T>
    void Literal<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
}
