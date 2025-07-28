#pragma once

namespace demiplane::db {
    template <typename T>
    Field& Field::set(T&& value) {
        value_ = std::forward<T>(value);
        return *this;
    }

    template <typename T>
    const T& Field::get() const {
        if (!std::holds_alternative<T>(value_)) {
            throw std::bad_variant_access();
        }
        return std::get<T>(value_);
    }

    template <typename T>
    std::optional<T> Field::try_get() const {
        if (std::holds_alternative<T>(value_)) {
            return std::get<T>(value_);
        }
        return std::nullopt;
    }
}