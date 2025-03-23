#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "db_base.hpp"
#include "db_config_interface.hpp"
#include "db_connect_params.hpp"
#include "ires.hpp"
#include "scroll_tracer.hpp"
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

        DbInterface(const ConnectParams& connect_params, std::unique_ptr<scroll::TracerInterface> tracer)
            : connect_params_(connect_params), tracer_(std::move(tracer)) {}
        explicit DbInterface(const ConnectParams& params) : connect_params_(params) {
            // TODO: change to file or come up with good def tracer
            tracer_ = scroll::TracerFactory::create_default_console_tracer<>();
        }
        DbInterface() = default;

        virtual IRes<> create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) = 0;
        // Transaction Methods
        virtual IRes<> start_transaction() = 0;

        virtual IRes<> commit_transaction() = 0;

        virtual IRes<> rollback_transaction() = 0;

        virtual IRes<> connect(const ConnectParams &params) {
            connect_params_ = params;
            return {};
        }
        virtual IRes<> drop_connect() = 0;

        // Table Management
        virtual IRes<> create_table(const query::CreateQuery& proposal) = 0;

        virtual IRes<> delete_table(std::string_view table_name) = 0;

        virtual IRes<> truncate_table(std::string_view table_name) = 0;

        [[nodiscard]] virtual IRes<bool> check_table(std::string_view table_name) = 0;

        virtual IRes<> make_unique_constraint(std::string_view table_name, FieldCollection key_fields) = 0;

        [[nodiscard]] virtual IRes<> setup_search_index(std::string_view table_name, FieldCollection fields) = 0;

        /// @brief Drop index, but doesn't remove fts fields from this client.
        /// Allows restoring it(reindex) using restore_full_text_search method
        [[nodiscard]] virtual IRes<> drop_search_index(std::string_view table_name) const = 0;

        /// @brief Drop index + remove fields from this client. For using fts further setup_fulltext_search should be
        /// called again
        [[nodiscard]] virtual IRes<> remove_search_index(std::string_view table_name) = 0;

        /// @brief Restore index + reindex. Use previous declared fts fields
        [[nodiscard]] virtual IRes<> restore_search_index(std::string_view table_name) const = 0;

        // Data Manipulation using Perfect Forwarding

        virtual IRes<> insert(query::InsertQuery&& query) = 0;


        virtual IRes<> upsert(query::UpsertQuery&& query) = 0;

        virtual IRes<Records> insert_with_returning(query::InsertQuery&& query) = 0;

        virtual IRes<Records> upsert_with_returning(query::UpsertQuery&& query) = 0;
        // Data Retrieval
        [[nodiscard]] virtual IRes<Records> select(const query::SelectQuery& conditions) const = 0;

        // Remove Data
        virtual IRes<> remove(const query::DeleteQuery& conditions) = 0;

        [[nodiscard]] virtual IRes<uint32_t> count(const query::CountQuery& conditions) const = 0;

        virtual void set_search_fields(std::string_view table_name, FieldCollection fields) noexcept = 0;

        virtual void set_conflict_fields(std::string_view table_name, FieldCollection fields) noexcept = 0;
    protected:
        ConnectParams connect_params_;
        std::unique_ptr<scroll::TracerInterface> tracer_;
    };
} // namespace demiplane::database
