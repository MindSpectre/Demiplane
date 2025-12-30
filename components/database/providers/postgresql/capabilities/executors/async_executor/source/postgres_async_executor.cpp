#include "postgres_async_executor.hpp"

#include <boost/asio/redirect_error.hpp>

#include "log_macros.hpp"

namespace asio = boost::asio;

namespace demiplane::db::postgres {

    AsyncExecutor::AsyncExecutor(PGconn* conn, executor_type executor)
        : conn_{conn},
          executor_{std::move(executor)} {
        // Use check_connection for validation
        if (const auto ec = check_connection(conn_); !ec.is_success()) {
            throw std::runtime_error("AsyncExecutor: " + extract_connection_error(conn_).format());
        }

        if (PQsetnonblocking(conn_, 1) != 0) {
            throw std::runtime_error("AsyncExecutor: failed to set non-blocking mode - " +
                                     std::string(PQerrorMessage(conn_)));
        }

        cached_socket_fd_ = PQsocket(conn_);
        if (cached_socket_fd_ < 0) {
            throw std::runtime_error("AsyncExecutor: invalid socket descriptor");
        }

        socket_ = std::make_unique<asio::posix::stream_descriptor>(executor_, cached_socket_fd_);
    }

    AsyncExecutor::~AsyncExecutor() {
        if (socket_ && socket_->is_open()) {
            socket_->release();
        }
    }

    AsyncExecutor::AsyncExecutor(AsyncExecutor&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          executor_{std::move(other.executor_)},
          socket_{std::move(other.socket_)},
          cached_socket_fd_{std::exchange(other.cached_socket_fd_, -1)} {
    }

    AsyncExecutor& AsyncExecutor::operator=(AsyncExecutor&& other) noexcept {
        if (this != &other) {
            if (socket_ && socket_->is_open()) {
                socket_->release();
            }
            conn_             = std::exchange(other.conn_, nullptr);
            executor_         = std::move(other.executor_);
            socket_           = std::move(other.socket_);
            cached_socket_fd_ = std::exchange(other.cached_socket_fd_, -1);
        }
        return *this;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Validation
    // ─────────────────────────────────────────────────────────────────────────────

    std::optional<ErrorContext> AsyncExecutor::validate_state() const {
        // Quick health check first
        if (const auto ec = check_connection(conn_); !ec.is_success()) {
            // Get full error context for bad connection
            if (conn_) {
                return extract_connection_error(conn_);
            }
            // conn_ is null - build minimal context
            ErrorContext ctx{ErrorCode{ClientErrorCode::NotConnected}};
            ctx.message = "Connection is null";
            return ctx;
        }

        if (!socket_ || !socket_->is_open()) {
            ErrorContext ctx{ErrorCode{ClientErrorCode::NotConnected}};
            ctx.message = "Socket descriptor not available";
            return ctx;
        }

        // Detect connection reset (fd changed under us)
        if (const int current_fd = PQsocket(conn_); current_fd != cached_socket_fd_) {
            ErrorContext ctx{ErrorCode{ClientErrorCode::InvalidState}};
            ctx.message = "Connection was reset (socket fd changed)";
            ctx.detail  = "Expected fd " + std::to_string(cached_socket_fd_) + ", got " + std::to_string(current_fd);
            return ctx;
        }

        return std::nullopt;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Public execute overloads
    // ─────────────────────────────────────────────────────────────────────────────

    asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
    AsyncExecutor::execute(const std::string_view query) const {
        co_return co_await execute_impl(query.data(), nullptr);
    }

    asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>> AsyncExecutor::execute(const std::string_view query,
                                                                                      const Params& params) const {
        co_return co_await execute_impl(query.data(), &params);
    }

    asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
    AsyncExecutor::execute(const CompiledQuery& query) const {
        if (query.provider() != SupportedProviders::PostgreSQL) {
            ErrorContext ctx{ErrorCode{ClientErrorCode::SyntaxError}};
            ctx.message = "Query compiled for different provider";
            ctx.detail  = "Expected PostgreSQL, got different backend";
            co_return gears::Err(std::move(ctx));
        }

        if (const auto params_ptr = query.backend_packet_as<Params>()) {
            co_return co_await execute_impl(query.sql().data(), params_ptr.get());
        }
        co_return co_await execute_impl(query.sql().data(), nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Core implementation
    // ─────────────────────────────────────────────────────────────────────────────

    asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
    AsyncExecutor::execute_with_resources(const std::string_view query,
                                          const std::shared_ptr<std::pmr::memory_resource> pool,
                                          const std::shared_ptr<Params> params) const {
        gears::unused_value(pool);
        // pool and params are on coroutine frame (copied as shared_ptr before initial_suspend)
        // They keep memory alive across all suspension points
        co_return co_await execute(query, *params);
    }

    asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>> AsyncExecutor::execute_impl(const char* query,
                                                                                           const Params* params) const {
        COMPONENT_LOG_ENTER_FUNCTION();

        // 1. Validate executor state
        if (auto err = validate_state()) {
            COMPONENT_LOG_ERR() << "Validation failed: " << *err;
            co_return gears::Err(std::move(*err));
        }

        // 2. Send query
        const int send_ok = params && !params->values.empty()
                                ? PQsendQueryParams(conn_,
                                                    query,
                                                    static_cast<int>(params->values.size()),
                                                    params->oids.data(),
                                                    params->values.data(),
                                                    params->lengths.data(),
                                                    params->formats.data(),
                                                    1  // Binary results
                                                    )
                                : PQsendQuery(conn_, query);

        if (send_ok == 0) {
            // Use extract_connection_error for send failures
            auto err = extract_connection_error(conn_);
            COMPONENT_LOG_ERR() << "Send failed: " << err;
            co_return gears::Err(std::move(err));
        }

        // 3. Flush output buffer
        if (auto err = co_await async_flush()) {
            COMPONENT_LOG_ERR() << "Flush failed: " << *err;
            co_return gears::Err(std::move(*err));
        }

        // 4. Wait for results
        if (auto err = co_await async_consume_until_ready()) {
            COMPONENT_LOG_ERR() << "Consume failed: " << *err;
            co_return gears::Err(std::move(*err));
        }

        // 5. Collect result
        co_return collect_single_result();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Async primitives
    // ─────────────────────────────────────────────────────────────────────────────

    asio::awaitable<std::optional<ErrorContext>> AsyncExecutor::async_flush() const {
        while (true) {
            const int flush_result = PQflush(conn_);

            if (flush_result == 0) {
                co_return std::nullopt;  // Flush complete
            }

            if (flush_result < 0) {
                // libpq error during flush
                co_return extract_connection_error(conn_);
            }

            // Need to wait for socket writable
            boost::system::error_code ec;
            co_await socket_->async_wait(asio::posix::stream_descriptor::wait_write,
                                         asio::redirect_error(asio::use_awaitable, ec));

            if (ec) {
                ErrorContext ctx{ErrorCode{ServerErrorCode::RuntimeError}};
                ctx.message = "Socket write wait failed";
                ctx.detail  = ec.message();
                co_return ctx;
            }
        }
    }

    asio::awaitable<std::optional<ErrorContext>> AsyncExecutor::async_consume_until_ready() const {
        while (true) {
            boost::system::error_code ec;
            co_await socket_->async_wait(asio::posix::stream_descriptor::wait_read,
                                         asio::redirect_error(asio::use_awaitable, ec));

            if (ec) {
                ErrorContext ctx{ErrorCode{ServerErrorCode::RuntimeError}};
                ctx.message = "Socket read wait failed";
                ctx.detail  = ec.message();
                co_return ctx;
            }

            // Consume available data from socket
            if (PQconsumeInput(conn_) == 0) {
                co_return extract_connection_error(conn_);
            }

            // Check if query processing complete
            if (PQisBusy(conn_) == 0) {
                co_return std::nullopt;
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Result collection
    // ─────────────────────────────────────────────────────────────────────────────

    gears::Outcome<ResultBlock, ErrorContext> AsyncExecutor::collect_single_result() const {
        PGresult* result = PQgetResult(conn_);

        if (!result) {
            ErrorContext ctx{ErrorCode{ClientErrorCode::InvalidArgument}};
            ctx.message = "No result returned from query";
            return gears::Err(std::move(ctx));
        }

        // Drain additional results (protocol cleanup)
        while (PGresult* extra = PQgetResult(conn_)) {
            PQclear(extra);
        }

        // Success - process_result handles PGresult -> ResultBlock conversion
        // It will call PQclear internally
        return process_result(result);
    }

}  // namespace demiplane::db::postgres
