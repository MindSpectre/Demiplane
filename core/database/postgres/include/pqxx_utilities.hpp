#pragma once
#include <thread>


namespace demiplane::database::utilities {
    inline void multi_thread_insertion(const std::shared_ptr<PqxxClient>& client, const std::string_view table_name,
        std::vector<Record>&& records, const uint32_t flush = 1 << 10, const int8_t thread_count = 4) {
        auto poster_worker = [&](const int start_index) {
            query::InsertQuery query;
            std::vector<Record> pack;
            pack.reserve(flush);
            for (uint32_t i = start_index; i < records.size(); i += thread_count) {
                pack.push_back(std::move(records[i]));
                if (pack.size() >= flush) {
                    query.to(table_name).insert(std::move(pack));
                    client->insert(std::move(query));
                    pack.clear();
                }
            }
            if (!pack.empty()) {
                query.to(table_name).insert(std::move(pack));
                client->insert(std::move(query));
                pack.clear();
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        client->start_transaction();
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back(poster_worker, t); // Each thread handles a different starting point
        }
        for (auto& thread : threads) {
            thread.join();
        }
        client->commit_transaction();
    }

    inline void bulk_insertion(const std::shared_ptr<PqxxClient>& client, const std::string_view table_name,
        std::vector<Record>&& records, const uint32_t flush = 1 << 14, const int8_t thread_count = 4) {
        auto poster_worker = [&](const int start_index) {
            query::InsertQuery query;
            std::vector<Record> pack;
            pack.reserve(flush);
            for (uint32_t i = start_index; i < records.size(); i += thread_count) {
                pack.push_back(std::move(records[i]));
                if (pack.size() >= flush) {
                    query.to(table_name).insert(std::move(pack));
                    client->insert(std::move(query));
                    pack.clear();
                }
            }
            if (!pack.empty()) {
                query.to(table_name).insert(std::move(pack));
                client->insert(std::move(query));
                pack.clear();
            }
        };

        std::vector<std::thread> threads;
        threads.reserve(thread_count);
        client->start_transaction();
        client->drop_search_index(table_name);
        for (int t = 0; t < thread_count; ++t) {
            threads.emplace_back(poster_worker, t); // Each thread handles a different starting point
        }
        for (auto& thread : threads) {
            thread.join();
        }
        client->restore_search_index(table_name);
        client->commit_transaction();
    }
} // namespace demiplane::database::utilities
