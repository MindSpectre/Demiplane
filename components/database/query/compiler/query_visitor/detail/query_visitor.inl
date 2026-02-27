#pragma once

namespace demiplane::db {
    constexpr void DynamicColumn::accept(this auto&& self, auto& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename T>
    constexpr void TableColumn<T>::accept(this auto&& self, auto& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    constexpr void AllColumns::accept(this auto&& self, auto& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename Derived>
    constexpr void Expression<Derived>::accept(this auto&& self, auto& visitor) {
        visitor.visit(std::forward<decltype(self)>(self).self());
    }

    template <typename T>
    constexpr void Literal<T>::accept(this auto&& self, auto& visitor) {
        visitor.visit(std::forward<decltype(self)>(self));
    }
}  // namespace demiplane::db
