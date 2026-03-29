#include "postgres_sync_executor.hpp"

namespace demiplane::db::postgres {

    SyncExecutor::SyncExecutor(PGconn* conn) noexcept
        : conn_{conn} {
    }

    SyncExecutor::SyncExecutor(ConnectionSlot& slot) noexcept
        : conn_{slot.conn},
          slot_{&slot} {
    }

    SyncExecutor::~SyncExecutor() {
        if (slot_) {
            slot_->reset();
        }
    }

    SyncExecutor::SyncExecutor(SyncExecutor&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          slot_{std::exchange(other.slot_, nullptr)} {
    }

    SyncExecutor& SyncExecutor::operator=(SyncExecutor&& other) noexcept {
        if (this != &other) {
            if (slot_) {
                slot_->reset();
            }
            conn_ = std::exchange(other.conn_, nullptr);
            slot_ = std::exchange(other.slot_, nullptr);
        }
        return *this;
    }

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::execute(const CompiledDynamicQuery& query) const {
        if (query.provider() != Providers::PostgreSQL) {
            ErrorContext ec{ErrorCode{ClientErrorCode::SyntaxError}};
            ec.context = "Wrong provider. Query was compiled not by PostgreSQL";
            return gears::Err(ec);
        }
        const auto params_ptr = query.backend_packet_as<Params>();
        if (!params_ptr) {
            return execute_impl(query.c_sql(), nullptr);
        }
        return execute_impl(query.c_sql(), params_ptr.get());
    }

    gears::Outcome<ResultBlock, ErrorContext> SyncExecutor::execute_impl(const char* query,
                                                                         const Params* params) const {
        COMPONENT_LOG_ENTER_FUNCTION();
        COMPONENT_LOG_TRC() << SCROLL_PARAMS(query, params);
        if (const auto ec = check_connection(conn_); ec) {
            ErrorContext ctx(ec);
            COMPONENT_LOG_ERR() << "Connection failed: " << ctx;
            return gears::Err(std::move(ctx));
        }

        PGresult* result = (params == nullptr || params->values.empty())
                               ? PQexec(conn_, query)
                               : PQexecParams(conn_,
                                              query,
                                              static_cast<int>(params->values.size()),
                                              params->oids.data(),
                                              params->values.data(),
                                              params->lengths.data(),
                                              params->formats.data(),
                                              1);

        if (!result) {
            auto ec = extract_connection_error(conn_);
            COMPONENT_LOG_ERR() << "Connection failed: " << ec;
            return gears::Err(std::move(ec));
        }

        return process_result(result);
    }
}  // namespace demiplane::db::postgres
