#pragma once

namespace demiplane::db {
    void DynamicColumn::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename T>
    void TableColumn<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    void AllColumns::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename Derived>
    void Expression<Derived>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self).self());
    }

    void Literal::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
}  // namespace demiplane::db
