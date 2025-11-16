#pragma once

#include <algorithm>

#include "../basic.hpp"

namespace demiplane::db {
    template <IsTable TableT>
    class InsertExpr : public Expression<InsertExpr<TableT>>, public TableHolder<TableT> {
    public:
        template <typename TableTp>
            requires std::constructible_from<TableT, TableTp>
        constexpr explicit InsertExpr(TableTp&& t) noexcept
            : TableHolder<TableT>{std::forward<TableTp>(t)} {
        }

        template <typename Self>
        constexpr decltype(auto) into(this Self&& self, const std::initializer_list<std::string> cols) noexcept {
            self.columns_ = cols;
            return std::forward<Self>(self);
        }

        template <typename Self>
        constexpr decltype(auto) values(this Self&& self, std::initializer_list<FieldValue> vals) noexcept {
            self.rows_.emplace_back(vals);
            return std::forward<Self>(self);
        }

        template <typename Self, typename RecordTp>
            requires std::same_as<std::remove_cvref_t<RecordTp>, Record>
        decltype(auto) values(this Self&& self, RecordTp&& record) noexcept {
            std::vector<FieldValue> row;
            row.reserve(self.columns_.size());
            for (const auto& col : self.columns_) {
                if constexpr (std::is_rvalue_reference_v<RecordTp&&>) {
                    row.push_back(std::move(record[col]).raw_value());
                } else {
                    row.push_back(record[col].raw_value());
                }
            }
            self.rows_.push_back(std::move(row));
            return std::forward<Self>(self);
        }

        template <typename Self, typename RecordsTp>
            requires std::same_as<std::remove_cvref_t<RecordsTp>, std::vector<Record>>
        decltype(auto) batch(this Self&& self, RecordsTp&& records) noexcept {
            if constexpr (std::is_rvalue_reference_v<RecordsTp&&>) {
                for (auto&& record : records) {
                    self.values(std::move(record));
                }
            } else {
                for (const auto& record : records) {
                    self.values(record);
                }
            }
            return std::forward<Self>(self);
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& columns(this Self&& self) noexcept {
            return std::forward<Self>(self).columns_;
        }

        template <typename Self>
        [[nodiscard]] constexpr auto&& rows(this Self&& self) noexcept {
            return std::forward<Self>(self).rows_;
        }

    private:
        std::vector<std::string> columns_;
        std::vector<std::vector<FieldValue>> rows_;
    };

    // INSERT builder function
    template <typename TablePtrTp>
        requires std::constructible_from<TablePtr, std::remove_cvref_t<TablePtrTp>>
    constexpr auto insert_into(TablePtrTp&& table) noexcept {
        return InsertExpr<TablePtr>{std::forward<TablePtrTp>(table)};
    }

    template <typename StringTp>
        requires std::constructible_from<std::string, std::remove_cvref_t<StringTp>>
    constexpr auto insert_into(StringTp&& table_name) noexcept {
        return InsertExpr<std::string>{std::forward<StringTp>(table_name)};
    }

    template <typename StringTp>
        requires std::is_same_v<std::remove_cvref_t<StringTp>, std::string_view> ||
                 std::is_same_v<std::remove_cvref_t<StringTp>, const char*>
    constexpr auto insert_into(StringTp&& table_name) noexcept {
        return InsertExpr<std::string_view>{std::forward<StringTp>(table_name)};
    }
}  // namespace demiplane::db
