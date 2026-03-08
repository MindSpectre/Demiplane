#pragma once

#include <string>
#include <tuple>

namespace demiplane::db {

    template <typename SqlStringT, typename... Params>
    struct CompiledStaticQuery {
        SqlStringT sql_;
        std::tuple<Params...> params_;

        [[nodiscard]] constexpr std::string_view sql() const noexcept {
            return std::string_view{sql_};
        }

        [[nodiscard]] constexpr const std::tuple<Params...>& params() const noexcept {
            return params_;
        }

        [[nodiscard]] static constexpr std::size_t size() noexcept {
            return sizeof...(Params);
        }
    };

    // Deduction guide
    template <typename SqlStringT, typename... Params>
    CompiledStaticQuery(SqlStringT, std::tuple<Params...>) -> CompiledStaticQuery<SqlStringT, Params...>;

}  // namespace demiplane::db
