#pragma once

#include <gears_concepts.hpp>

#include "../basic.hpp"

namespace demiplane::db {

    template <IsTable TableT>
    class DropTableExpr : public Expression<DropTableExpr<TableT>>, public TableHolder<TableT> {
    public:
        template <typename TableTp>
            requires std::constructible_from<TableT, TableTp>
        constexpr explicit DropTableExpr(TableTp&& t, const bool if_exists = false, const bool cascade = false) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)},
              if_exists_{if_exists},
              cascade_{cascade} {
        }

        [[nodiscard]] constexpr bool if_exists() const noexcept {
            return if_exists_;
        }

        [[nodiscard]] constexpr bool cascade() const noexcept {
            return cascade_;
        }

    private:
        bool if_exists_ = false;
        bool cascade_   = false;
    };

    // Factory 1: TablePtr
    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrTp>> &&
                 (!gears::IsStringLike<TablePtrTp>)
    constexpr auto drop_table(TablePtrTp&& table, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<TablePtr>{std::forward<TablePtrTp>(table), if_exists, cascade};
    }

    // Factory 2: std::string
    template <typename StringTp>
        requires gears::IsStringLike<StringTp> && (!gears::IsStringViewLike<StringTp>)
    constexpr auto
    drop_table(StringTp&& table_name, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<std::string>{std::forward<StringTp>(table_name), if_exists, cascade};
    }

    // Factory 3: string_view
    template <typename StringTp>
        requires gears::IsStringViewLike<StringTp>
    constexpr auto
    drop_table(StringTp&& table_name, const bool if_exists = false, const bool cascade = false) noexcept {
        return DropTableExpr<std::string_view>{std::forward<StringTp>(table_name), if_exists, cascade};
    }

}  // namespace demiplane::db
