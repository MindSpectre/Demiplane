#pragma once

#include <db_field.hpp>

namespace demiplane::db {
    class Block {
    public:
        virtual ~Block() = default;

        [[nodiscard]] virtual const Field& operator[](std::size_t row, std::size_t column) const = 0;
        [[nodiscard]] virtual Field operator[](std::size_t row, std::size_t column)              = 0;
    };
}  // namespace demiplane::db
