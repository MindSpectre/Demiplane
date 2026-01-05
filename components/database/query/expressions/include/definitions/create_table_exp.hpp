#pragma once

#include <gears_concepts.hpp>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsTable TableT>
    class CreateTableExpr : public Expression<CreateTableExpr<TableT>>, public TableHolder<TableT> {
    public:
        template <IsTable TableTp = std::remove_cvref_t<TableT>>
        constexpr explicit CreateTableExpr(TableTp&& t, const bool if_not_exists = false) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)},
              if_not_exists_{if_not_exists} {
        }

        [[nodiscard]] constexpr bool if_not_exists() const noexcept {
            return if_not_exists_;
        }

    private:
        bool if_not_exists_ = false;
    };

    // Factory: TablePtr (requires full schema for DDL generation)
    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrTp>> &&
                 (!gears::IsStringLike<TablePtrTp>)
    constexpr auto create_table(TablePtrTp&& table, const bool if_not_exists = false) noexcept {
        return CreateTableExpr<TablePtr>{std::forward<TablePtrTp>(table), if_not_exists};
    }

}  // namespace demiplane::db
