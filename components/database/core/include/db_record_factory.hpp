#pragma once

#include "db_record.hpp"

namespace demiplane::db {
    class RecordFactory {
        public:
        explicit RecordFactory(std::shared_ptr<const TableSchema> schema)
            : schema_(std::move(schema)) {
        }

        [[nodiscard]] Record create_record() const {
            return Record{schema_};
        }

        std::vector<Record> create_batch(size_t count) {
            std::vector<Record> records;
            records.reserve(count);

            for (size_t i = 0; i < count; ++i) {
                records.emplace_back(schema_);
            }

            return records;
        }

        [[nodiscard]] const TableSchema& schema() const {
            return *schema_;
        }

        private:
        std::shared_ptr<const TableSchema> schema_;
    };
}  // namespace demiplane::db
