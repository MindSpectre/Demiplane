#pragma once

#include <algorithm>

#include "db_record.hpp"
#include "../basic.hpp"

namespace demiplane::db {
    class InsertExpr : public Expression<InsertExpr> {
    public:
        explicit InsertExpr(TableSchemaPtr t)
            : table_(std::move(t)) {}

        // Set columns
        InsertExpr& into(std::initializer_list<std::string> cols) {
            columns_ = cols;
            return *this;
        }

        // Add single value row
        InsertExpr& values(std::initializer_list<FieldValue> vals) {
            rows_.emplace_back(vals);
            return *this;
        }

        // Add row from Record
        InsertExpr& values(const Record& record) {
            std::vector<FieldValue> row;
            row.reserve(columns_.size());
            for (const auto& col : columns_) {
                row.push_back(record[col].raw_value());
            }
            rows_.push_back(std::move(row));
            return *this;
        }

        // Batch insert
        InsertExpr& batch(const std::vector<Record>& records) {
            for (const auto& record : records) {
                values(record);
            }
            return *this;
        }

        [[nodiscard]] const TableSchemaPtr& table() const {
            return table_;
        }

        [[nodiscard]] const std::vector<std::string>& columns() const {
            return columns_;
        }

        [[nodiscard]] const std::vector<std::vector<FieldValue>>& rows() const {
            return rows_;
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

    inline auto insert_into(const std::string_view table_name) {
        TableSchemaPtr schema = std::make_shared<TableSchema>(table_name);
        return InsertExpr{std::move(schema)};
    }
}
