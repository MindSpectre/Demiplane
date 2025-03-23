#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "db_base.hpp"
#include "db_connect_params.hpp"
#include "db_config_interface.hpp"
#include "traits_classes.hpp"
namespace demiplane::database {
    using namespace demiplane::database;
    template <typename T>
    concept RecordContainer = std::is_same_v<std::remove_cvref_t<T>, Records>;
    template <typename T>
    concept FieldBaseVector = std::is_same_v<std::remove_cvref_t<T>, std::vector<std::unique_ptr<FieldBase>>>;



    class DbInterface : NonCopyable {
    public:
        virtual ~DbInterface() = default;

        explicit DbInterface(const ConnectParams& params) : connect_params_(params) {}
        DbInterface() = default;

        virtual void create_database(const std::shared_ptr<DatabaseConfig> &config, ConnectParams &pr) = 0;
        // Transaction Methods
        virtual void start_transaction() = 0;

        virtual void commit_transaction() = 0;

        virtual void rollback_transaction() = 0;

        virtual void connect(const ConnectParams &params) {
            connect_params_ = params;
        }
        virtual void drop_connect() = 0;

        // Table Management
        virtual void create_table(const query::CreateQuery& proposal) = 0;

        virtual void delete_table(std::string_view table_name) = 0;

        virtual void truncate_table(std::string_view table_name) = 0;

        [[nodiscard]] virtual bool check_table(std::string_view table_name) = 0;

        virtual void make_unique_constraint(std::string_view table_name, FieldCollection key_fields) = 0;

        virtual void setup_search_index(std::string_view table_name, FieldCollection fields) = 0;

        /// @brief Drop index, but doesn't remove fts fields from this client.
        /// Allows restoring it(reindex) using restore_full_text_search method
        virtual void drop_search_index(std::string_view table_name) const = 0;

        /// @brief Drop index + remove fields from this client. For using fts further setup_fulltext_search should be
        /// called again
        virtual void remove_search_index(std::string_view table_name) = 0;

        /// @brief Restore index + reindex. Use previous declared fts fields
        virtual void restore_search_index(std::string_view table_name) const = 0;

        // Data Manipulation using Perfect Forwarding

        virtual void insert(query::InsertQuery&& query) = 0;


        virtual void upsert(query::UpsertQuery&& query) = 0;

        virtual Records insert_with_returning(query::InsertQuery&& query) = 0;

        virtual Records upsert_with_returning(query::UpsertQuery&& query) = 0;
        // Data Retrieval
        [[nodiscard]] virtual Records select(const query::SelectQuery& conditions) const = 0;

        // Remove Data
        virtual void remove(const query::DeleteQuery& conditions) = 0;

        [[nodiscard]] virtual uint32_t count(const query::CountQuery& conditions) const = 0;

        virtual void set_search_fields(std::string_view table_name, FieldCollection fields) noexcept = 0;

        virtual void set_conflict_fields(std::string_view table_name, FieldCollection fields) noexcept = 0;
    protected:
        ConnectParams connect_params_;
    };
} // namespace demiplane::database
