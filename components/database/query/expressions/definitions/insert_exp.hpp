#pragma once

#include <algorithm>

#include "db_record.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    class InsertExpr : public Expression<InsertExpr> {
    public:
        explicit InsertExpr(TableSchemaPtr t)
            : table_(std::move(t)) {}

        InsertExpr& into(std::initializer_list<std::string> cols) {
            columns_ = cols;
            return *this;
        }

        InsertExpr& values(std::initializer_list<FieldValue> vals) {
            rows_.emplace_back(vals);
            return *this;
        }

        InsertExpr& values(const Record& record) {
            std::vector<FieldValue> row;
            row.reserve(columns_.size());
            for (const auto& col : columns_) {
                row.push_back(record[col].raw_value());
            }
            rows_.push_back(std::move(row));
            return *this;
        }

        InsertExpr& values(Record&& record) {
            std::vector<FieldValue> row;
            row.reserve(columns_.size());
            for (const auto& col : columns_) {
                row.push_back(std::move(record[col]).raw_value());
            }
            rows_.push_back(std::move(row));
            return *this;
        }

        InsertExpr& batch(const std::vector<Record>& records) {
            for (const auto& record : records) {
                values(record);
            }
            return *this;
        }

        InsertExpr& batch(std::vector<Record>&& records) {
            for (auto&& record : records) {
                values(std::move(record));
            }
            return *this;
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

        template <typename Self>
        [[nodiscard]] auto&& columns(this Self&& self) {
            return std::forward<Self>(self).columns_;
        }

        template <typename Self>
        [[nodiscard]] auto&& rows(this Self&& self) {
            return std::forward<Self>(self).rows_;
        }

    private:
        TableSchemaPtr table_{nullptr};
        std::vector<std::string> columns_;
        std::vector<std::vector<FieldValue>> rows_;
    };

    // INSERT builder function
    inline auto insert_into(TableSchemaPtr table) {
        return InsertExpr{std::move(table)};
    }

    inline auto insert_into(std::string table_name) {
        return InsertExpr{TableSchema::make_ptr(std::move(table_name))};
    }
}
