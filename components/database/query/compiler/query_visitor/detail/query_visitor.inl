#pragma once

namespace demiplane::db {
    constexpr decltype(auto) Column::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self));
    }

    constexpr decltype(auto) AllColumns::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename CppType>
    constexpr decltype(auto) TypedColumn<CppType>::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename Derived>
    constexpr decltype(auto) Expression<Derived>::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self).self());
    }

    template <typename T>
    constexpr decltype(auto) Literal<T>::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self));
    }

    template <typename T>
    constexpr decltype(auto) ParamPlaceholder<T>::accept(this auto&& self, auto& visitor) {
        return visitor.visit(std::forward<decltype(self)>(self));
    }
}  // namespace demiplane::db
