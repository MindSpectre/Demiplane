#pragma once

#include <string>
#include <tuple>

#include "gears_exception_result.hpp"
#include "gears_utils.hpp"

namespace demiplane::db {

    // TODO:DOC: SqlStringT is only inline for compiler
    template <typename SqlStringT, typename... Params>
    struct CompiledStaticQuery {
        SqlStringT sql_;
        std::tuple<Params...> params_;

        [[nodiscard]] constexpr std::string_view sql() const noexcept {
            return std::string_view{sql_};
        }

        [[nodiscard]] constexpr const char* c_sql() const noexcept {
            return sql_.c_str();
        }

        [[nodiscard]] constexpr const std::tuple<Params...>& params() const noexcept {
            return params_;
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept {
            gears::force_non_static(this);
            return sizeof...(Params);
        }
    };

    // Deduction guide
    template <typename SqlStringT, typename... Params>
    CompiledStaticQuery(SqlStringT, std::tuple<Params...>) -> CompiledStaticQuery<SqlStringT, Params...>;

}  // namespace demiplane::db
