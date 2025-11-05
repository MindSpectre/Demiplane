#pragma once
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <gears_outcome.hpp>
#include <libpq-fe.h>
#include <postgres_result.hpp>

#include "compiled_query.hpp"
#include "db_error_codes.hpp"
#include "postgres_params.hpp"
namespace demiplane::db::postgres {
    class AsyncExecutor {
    public:
        AsyncExecutor(PGconn* conn, boost::asio::io_context& io)
            : conn_(conn),
              io_(io),
              socket_(io, PQsocket(conn)) {
            PQsetnonblocking(conn_, 1);
        }

        // Simple query without params
        boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorCode>> execute(std::string_view query) {
            co_return co_await execute(query, {});
        }

        // Query with params
        boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorCode>> execute(std::string_view query,
                                                                               const Params& params) {
            // Send query
            int status = params.values.empty() ? PQsendQuery(conn_, query.data())
                                               : PQsendQueryParams(conn_,
                                                                   query.data(),
                                                                   static_cast<int>(params.values.size()),
                                                                   params.oids.data(),
                                                                   params.values.data(),
                                                                   params.lengths.data(),
                                                                   params.formats.data(),
                                                                   1);  // binary result format

            if (!status) {
                co_return ErrorCode::SendFailed;
            }

            // Flush and wait
            co_await flush_async();

            // Collect results
            co_return co_await consume_results();
        }

        // CompiledQuery overload
        boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorCode>> execute(const CompiledQuery& query,
                                                                               const Params& params) {
            co_return co_await execute(query.sql(), params);
        }

    private:
        PGconn* conn_;
        boost::asio::io_context& io_;
        boost::asio::posix::stream_descriptor socket_;

        boost::asio::awaitable<void> flush_async() {
            while (PQflush(conn_) > 0) {
                co_await socket_.async_wait(boost::asio::posix::stream_descriptor::wait_write,
                                            boost::asio::use_awaitable);
            }
        }

        boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorCode>> consume_results() {
            // Wait for read ready
            co_await socket_.async_wait(boost::asio::posix::stream_descriptor::wait_read, boost::asio::use_awaitable);

            // Consume input
            if (!PQconsumeInput(conn_)) {
                co_return ;
            }

            // Collect all result sets
            std::vector<PGresult*> results;
            while (PQisBusy(conn_) == 0) {
                PGresult* res = PQgetResult(conn_);
                if (!res)
                    break;  // No more results

                results.push_back(res);
            }

            // If still busy, need to wait more
            if (PQisBusy(conn_)) {
                co_return co_await consume_results();  // Recursive wait
            }

            // Check results
            if (results.empty()) {
                co_return ErrorCode::NoResult;  // ?
            }

            // Take last result (others might be from multi-statement)
            auto last_result = results.back();

            // Clean up intermediate results
            for (size_t i = 0; i < results.size() - 1; ++i) {
                PQclear(results[i]);
            }

            // Check status
            auto status = PQresultStatus(last_result);
            if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
                ErrorCode ec = map_postgres_error(status);
                PQclear(last_result);
                co_return gears::Err(ec);
            }

            co_return ResultBlock(last_result);  // Your Result wrapper takes ownership
        }
    };
}  // namespace demiplane::db::postgres
