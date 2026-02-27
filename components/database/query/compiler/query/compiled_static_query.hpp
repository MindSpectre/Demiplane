#pragma once

#include <string>
#include <tuple>

namespace demiplane::db {

    template <typename... Params>
    struct CompiledStaticQuery {
        std::string sql_;
        std::tuple<Params...> params_;

        [[nodiscard]] constexpr std::string_view sql() const noexcept {
            return sql_;
        }

        [[nodiscard]] constexpr const std::tuple<Params...>& params() const noexcept {
            return params_;
        }

        [[nodiscard]] static constexpr std::size_t size() noexcept {
            return sizeof...(Params);
        }
    };

    // Deduction guide
    template <typename... Params>
    CompiledStaticQuery(std::string, std::tuple<Params...>) -> CompiledStaticQuery<Params...>;

}  // namespace demiplane::db
