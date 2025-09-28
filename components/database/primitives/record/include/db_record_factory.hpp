#pragma once

#include "db_record.hpp"

namespace demiplane::db {
    /**
     * @brief Factory for creating Record instances that share a common schema.
     *
     * Provides efficient Record creation by sharing a single Table schema instance
     * across multiple Record objects, avoiding schema duplication.
     */
    class RecordFactory {
    public:
        /**
         * @brief Constructs a RecordFactory with the specified schema.
         *
         * @param schema Shared schema for all Records created by this factory.
         *               Must remain valid for the lifetime of created Records.
         */
        explicit RecordFactory(std::shared_ptr<const Table> schema)
            : schema_(std::move(schema)) {
        }

        /**
         * @brief Creates a single Record instance with the factory's schema.
         *
         * @return Record instance initialized with the shared schema.
         *
         * @attention Multiple Records created by this factory share the same
         *            schema instance for memory efficiency.
         */
        [[nodiscard]] Record create_record() const {
            return Record{schema_};
        }

        /**
         * @brief Creates a batch of Record instances with pre-allocated capacity.
         *
         * @param count Number of Records to create.
         *
         * @return Vector containing the requested number of Records,
         *         all sharing the factory's schema.
         *
         * @note Vector capacity is pre-reserved to avoid reallocations
         *       during batch creation.
         */
        std::vector<Record> create_batch(const size_t count) {
            std::vector<Record> records;
            records.reserve(count);

            for (size_t i = 0; i < count; ++i) {
                records.emplace_back(schema_);
            }

            return records;
        }

        /**
         * @brief Provides access to the factory's schema.
         *
         * @return Reference to the Table schema used by this factory.
         *
         * @pre Factory must have been initialized with a valid schema.
         */
        [[nodiscard]] const Table& schema() const {
            return *schema_;
        }

    private:
        std::shared_ptr<const Table> schema_;
    };
}  // namespace demiplane::db
