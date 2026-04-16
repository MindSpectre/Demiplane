#include "postgres_async_executor.hpp"

#include <demiplane/scroll>

#include <boost/asio/redirect_error.hpp>

namespace demiplane::db::postgres {

    AsyncExecutor::AsyncExecutor(PGconn* conn, boost::asio::any_io_executor executor)
        : conn_{conn},
          executor_{std::move(executor)} {
        if (!conn_) {
            return;  // empty state — valid() returns false
        }
        setup_connection();
    }

    AsyncExecutor::AsyncExecutor(std::weak_ptr<ConnectionHolder> holder, boost::asio::any_io_executor executor)
        : executor_{std::move(executor)},
          holder_{std::move(holder)} {
        if (auto live = holder_.lock()) {
            conn_ = live->conn();
        }
        if (!conn_) {
            return;  // empty state — valid() returns false
        }
        setup_connection();
    }

    AsyncExecutor::~AsyncExecutor() {
        if (socket_ && socket_->is_open()) {
            socket_->release();
        }
        if (auto live = holder_.lock()) {
            live->reset();
        }
    }

    AsyncExecutor::AsyncExecutor(AsyncExecutor&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          executor_{std::move(other.executor_)},
          socket_{std::move(other.socket_)},
          cached_socket_fd_{std::exchange(other.cached_socket_fd_, -1)},
          holder_{std::move(other.holder_)} {
        other.holder_.reset();
    }

    AsyncExecutor& AsyncExecutor::operator=(AsyncExecutor&& other) noexcept {
        if (this != &other) {
            if (socket_ && socket_->is_open()) {
                socket_->release();
            }
            if (auto live = holder_.lock()) {
                live->reset();
            }
            conn_             = std::exchange(other.conn_, nullptr);
            executor_         = std::move(other.executor_);
            socket_           = std::move(other.socket_);
            cached_socket_fd_ = std::exchange(other.cached_socket_fd_, -1);
            holder_           = std::move(other.holder_);
            other.holder_.reset();
        }
        return *this;
    }

    void AsyncExecutor::setup_connection() {
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

        socket_ = std::make_unique<boost::asio::posix::stream_descriptor>(executor_, cached_socket_fd_);
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
    // Public execute overloads (non-template)
    // ─────────────────────────────────────────────────────────────────────────────

    boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
    AsyncExecutor::execute(const CompiledDynamicQuery& query) const {
        if (query.provider() != Providers::PostgreSQL) {
            ErrorContext ctx{ErrorCode{ClientErrorCode::SyntaxError}};
            ctx.message = "Query compiled for different provider";
            ctx.detail  = "Expected PostgreSQL, got different backend";
            co_return gears::Err(std::move(ctx));
        }

        if (const auto params_ptr = query.backend_packet_as<Params>()) {
            co_return co_await execute_impl(query.c_sql(), params_ptr.get());
        }
        co_return co_await execute_impl(query.c_sql(), nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // Core implementation
    // ─────────────────────────────────────────────────────────────────────────────

    boost::asio::awaitable<gears::Outcome<ResultBlock, ErrorContext>>
    AsyncExecutor::execute_impl(const char* query, const Params* params) const {
        COMPONENT_LOG_ENTER_FUNCTION();
        COMPONENT_LOG_TRC() << SCROLL_PARAMS(query, params);
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

    boost::asio::awaitable<std::optional<ErrorContext>> AsyncExecutor::async_flush() const {
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
            co_await socket_->async_wait(boost::asio::posix::stream_descriptor::wait_write,
                                         redirect_error(boost::asio::use_awaitable, ec));

            if (ec) {
                ErrorContext ctx{ErrorCode{ServerErrorCode::RuntimeError}};
                ctx.message = "Socket write wait failed";
                ctx.detail  = ec.message();
                co_return ctx;
            }
        }
    }

    boost::asio::awaitable<std::optional<ErrorContext>> AsyncExecutor::async_consume_until_ready() const {
        while (true) {
            boost::system::error_code ec;
            co_await socket_->async_wait(boost::asio::posix::stream_descriptor::wait_read,
                                         redirect_error(boost::asio::use_awaitable, ec));

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
