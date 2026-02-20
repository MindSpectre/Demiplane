#pragma once

namespace demiplane::db {
    template <typename StringType>
    void DynamicColumn<StringType>::accept(this auto&& self, QueryVisitor& visitor) {
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

    template <typename T>
    void Literal<T>::accept(this auto&& self, QueryVisitor& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
}  // namespace demiplane::db
