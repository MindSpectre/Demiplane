#pragma once

#include "arena.hpp"
#include "db_record.hpp"

namespace demiplane::db {
    class RecordFactory {
    public:
        explicit RecordFactory(std::shared_ptr<const TableSchema> schema, size_t arena_size = 1024 * 1024)
            : arena_(arena_size),
              schema_(std::move(schema)) {}

        [[nodiscard]] Record create_record() const {
            return Record{schema_};
        }


        //TODO: finish with arena usage
        // Batch creation for extreme performance
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

        void clear_arena() {
            arena_.clear();
        }

        [[nodiscard]] std::size_t arena_usage() const {
            return arena_.total_allocated();
        }

    private:
        Arena arena_;
        std::shared_ptr<const TableSchema> schema_;
    };
}
