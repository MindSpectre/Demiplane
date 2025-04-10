#pragma once

#include <memory>
#include <type_traits>
#include <vector>

#include "../db_config_interface.hpp"
#include "../db_connect_params.hpp"
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

    template <class Client>
    class DbInterface : public scroll::TracerProvider<Client>, NonCopyable {
    public:
        virtual ~DbInterface() = default;

        DbInterface(const ConnectParams& connect_params, std::shared_ptr<scroll::TracerInterface<Client>> tracer)
            : scroll::TracerProvider<Client>(std::move(tracer)), connect_params_(connect_params) {}
        explicit DbInterface(const ConnectParams& params) : connect_params_(params) {
            // TODO: change to file or come up with good def tracer
            this->i_tracer = scroll::TracerFactory::create_default_console_tracer<Client>();
        }
        DbInterface() = default;

        virtual Result create_database(const std::shared_ptr<DatabaseConfig>& config, const ConnectParams& pr) = 0;
        // Transaction Methods


        virtual Result connect(const ConnectParams& params) {
            connect_params_ = params;
            return Result::sOk();
        }
        virtual Result drop_connect() = 0;


        // Data Manipulation using Perfect Forwarding

        virtual Interceptor<std::optional<Records>> insert(query::InsertQuery&& query) = 0;

        virtual Interceptor<std::optional<Records>> upsert(query::UpsertQuery&& query) = 0;

        // Data Retrieval
        [[nodiscard]] virtual Interceptor<Records> select(const query::SelectQuery& conditions) const = 0;

        // Remove Data
        virtual Interceptor<std::optional<Records>> remove(const query::RemoveQuery& conditions) = 0;

        [[nodiscard]] virtual Interceptor<uint32_t> count(const query::CountQuery& conditions) const = 0;

    protected:
        [[nodiscard]] virtual std::exception_ptr analyze_exception(const std::exception& caught_exception) const = 0;

        ConnectParams connect_params_;
    };
} // namespace demiplane::database
