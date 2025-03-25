#pragma once

#include <memory>
#include <string_view>
#include <type_traits>
#include <vector>

#include "../db_config_interface.hpp"
#include "../db_connect_params.hpp"
#include "../traits/search_trait.hpp"
#include "../traits/table_management_trait.hpp"
#include "../traits/transaction_trait.hpp"
#include "../traits/unique_constraint_trait.hpp"
#include "db_base.hpp"
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


        virtual IRes<> connect(const ConnectParams& params) {
            connect_params_ = params;
            return {};
        }
        virtual IRes<> drop_connect() = 0;


        // Data Manipulation using Perfect Forwarding

        virtual IRes<std::optional<Records>> insert(query::InsertQuery&& query) = 0;

        virtual IRes<std::optional<Records>> upsert(query::UpsertQuery&& query) = 0;

        // Data Retrieval
        [[nodiscard]] virtual IRes<Records> select(const query::SelectQuery& conditions) const = 0;

        // Remove Data
        virtual IRes<std::optional<Records>> remove(const query::DeleteQuery& conditions) = 0;

        [[nodiscard]] virtual IRes<uint32_t> count(const query::CountQuery& conditions) const = 0;

    protected:
        ConnectParams connect_params_;
        std::unique_ptr<scroll::TracerInterface> tracer_;
    };
} // namespace demiplane::database
