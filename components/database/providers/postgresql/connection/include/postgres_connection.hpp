// postgres_connection.hpp

//TODO:!!!! REVIEW DESIGN AT LEAST TWICE
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <compiled_query.hpp>
#include <db_exceptions.hpp>
#include <gears_class_traits.hpp>
#include <libpq-fe.h>
#include <postgres_config.hpp>
#include <postgres_params.hpp>
#include <postgres_result.hpp>
namespace demiplane::db::postgres {

    namespace asio = boost::asio;

    enum class ConnectionStatus { DISCONNECTED, CONNECTING, CONNECTED, FAILED, IN_TRANSACTION, IN_PIPELINE };

    class Connection : public gears::NonCopyable {
    public:
        // Construction/Destruction
        explicit Connection(asio::io_context& io);
        Connection(asio::io_context& io, ConnectionConfig config);
        ~Connection() noexcept;

        // ===== Connection Management =====

        // Synchronous connect
        void connect();
        void connect(const ConnectionConfig& config);
        void disconnect() noexcept;
        void reset();  // Reset connection, keep config

        // Async connect
        asio::awaitable<void> async_connect();
        asio::awaitable<void> async_connect(const ConnectionConfig& config);
        asio::awaitable<void> async_disconnect();
        asio::awaitable<void> async_reset();

        // Status
        [[nodiscard]] bool is_connected() const noexcept;
        [[nodiscard]] ConnectionStatus status() const noexcept;
        [[nodiscard]] int backend_pid() const noexcept;
        [[nodiscard]] int server_version() const noexcept;
        [[nodiscard]] const ConnectionConfig& config() const noexcept {
            return config_;
        }
        [[nodiscard]] NodeRole role() const noexcept {
            return config_.role;
        }
        // ===== Query Execution (Synchronous) =====

        [[nodiscard]] ResultBlock exec(std::string_view sql);
        [[nodiscard]] std::vector<ResultBlock> exec_multi(std::string_view sql);

        [[nodiscard]] ResultBlock exec_params(std::string_view sql, const Params& params, bool binary_results = true);

        [[nodiscard]] ResultBlock
        exec_prepared(std::string_view stmt_name, const Params& params, bool binary_results = true);

        [[nodiscard]] ResultBlock exec(const CompiledQuery& query);

        [[nodiscard]] std::size_t exec_command(std::string_view sql);

        // ===== Query Execution (Asynchronous) =====

        [[nodiscard]] asio::awaitable<ResultBlock> async_exec(std::string_view sql);
        [[nodiscard]] asio::awaitable<std::vector<ResultBlock>> async_exec_multi(std::string_view sql);

        [[nodiscard]] asio::awaitable<ResultBlock>
        async_exec_params(std::string_view sql, const Params& params, bool binary_results = true);

        [[nodiscard]] asio::awaitable<ResultBlock>
        async_exec_prepared(std::string_view stmt_name, const Params& params, bool binary_results = true);

        [[nodiscard]] asio::awaitable<ResultBlock> async_exec(const CompiledQuery& query);

        // ===== Prepared Statements =====

        void prepare(std::string_view name, std::string_view sql, std::span<const Oid> param_types = {});
        asio::awaitable<void>
        async_prepare(std::string_view name, std::string_view sql, std::span<const Oid> param_types = {});

        void unprepare(std::string_view name);
        void unprepare_all();

        [[nodiscard]] bool is_prepared(std::string_view name) const;

        struct PreparedInfo {
            std::vector<Oid> param_types;
            std::vector<std::string> param_names;
            std::vector<Oid> result_types;
            std::vector<std::string> result_names;
        };
        [[nodiscard]] PreparedInfo describe_prepared(std::string_view name) const;

        // Auto-prepare cache
        void enable_auto_prepare(bool enable = true) {
            auto_prepare_ = enable;
        }
        [[nodiscard]] std::size_t auto_prepare_threshold() const {
            return auto_prepare_threshold_;
        }
        void set_auto_prepare_threshold(std::size_t n) {
            auto_prepare_threshold_ = n;
        }

        // ===== Transactions =====

        void begin(TransactionIsolation level = TransactionIsolation::READ_COMMITTED);
        void commit();
        void rollback();

        asio::awaitable<void> async_begin(TransactionIsolation level = TransactionIsolation::READ_COMMITTED);
        asio::awaitable<void> async_commit();
        asio::awaitable<void> async_rollback();

        // Savepoints
        void savepoint(std::string_view name);
        void release_savepoint(std::string_view name);
        void rollback_to_savepoint(std::string_view name);

        // 2PC support
        void prepare_transaction(std::string_view gid);
        void commit_prepared(std::string_view gid);
        void rollback_prepared(std::string_view gid);
        std::vector<std::string> list_prepared_transactions();

        [[nodiscard]] bool in_transaction() const noexcept;
        [[nodiscard]] char transaction_status() const noexcept;

        // RAII helpers


        // ===== COPY Operations =====

        class CopyIn;
        class CopyOut;

        [[nodiscard]] CopyIn copy_in(std::string_view sql);
        [[nodiscard]] CopyOut copy_out(std::string_view sql);

        [[nodiscard]] asio::awaitable<std::size_t>
        async_copy_from(std::string_view sql, std::function<asio::awaitable<std::optional<std::string>>()> read_chunk);

        [[nodiscard]] asio::awaitable<std::size_t>
        async_copy_to(std::string_view sql, std::function<asio::awaitable<void>(std::string_view)> write_chunk);

        // ===== Pipeline Mode (PG14+) =====

        void enter_pipeline_mode();
        void exit_pipeline_mode();
        void pipeline_sync();
        [[nodiscard]] bool is_pipeline_active() const;

        // Queue operations
        void send_query_params(std::string_view sql, const Params& params);
        void send_query_prepared(std::string_view name, const Params& params);

        // Async pipeline
        [[nodiscard]] asio::awaitable<std::vector<ResultBlock>> async_pipeline_execute(std::vector<CompiledQuery> queries);

        // ===== Notifications (LISTEN/NOTIFY) =====

        void listen(std::string_view channel);
        void unlisten(std::string_view channel);
        void unlisten_all();
        void notify(std::string_view channel, std::string_view payload = {});

        struct Notification {
            std::string channel;
            std::string payload;
            int backend_pid;
        };

        [[nodiscard]] std::optional<Notification> check_notification();
        [[nodiscard]] asio::awaitable<Notification> async_wait_notification();

        // Notification callback
        using NotificationHandler = std::function<void(const Notification&)>;
        void set_notification_handler(NotificationHandler handler);

        // ===== Large Objects =====

        class LargeObject;

        [[nodiscard]] LargeObject create_large_object();
        [[nodiscard]] LargeObject open_large_object(Oid oid, std::string_view mode = "r");
        void unlink_large_object(Oid oid);


        // Notice/warning handlers
        using NoticeHandler = std::function<void(std::string_view)>;
        void set_notice_handler(NoticeHandler handler);
        void set_warning_handler(NoticeHandler handler);

        // Cancel running query
        void cancel();
        [[nodiscard]] asio::awaitable<void> async_cancel();

    private:
        asio::io_context& io_;
        asio::posix::stream_descriptor socket_;
        ConnectionConfig config_;
        PGconn* conn_{nullptr};

        // State
        bool nonblocking_{false};
        bool auto_prepare_{false};
        std::size_t auto_prepare_threshold_{5};

        // Auto-prepare cache: SQL -> (prepared_name, execution_count)
        std::unordered_map<std::string, std::pair<std::string, std::size_t>> prepared_cache_;

        // Handlers
        NoticeHandler notice_handler_;
        NoticeHandler warning_handler_;
        NotificationHandler notification_handler_;

        // Async helpers
        asio::awaitable<void> wait_socket_readable();
        asio::awaitable<void> wait_socket_writable();
        asio::awaitable<ResultBlock> async_get_result();

        [[noreturn]] void throw_error(PGresult* result = nullptr);

        // Auto-prepare management
        void maybe_auto_prepare(std::string_view sql);
        std::string generate_prepared_name(std::string_view sql);
    };
    // COPY operations...
    class Connection::CopyIn {
    public:
        ~CopyIn() noexcept;
        CopyIn(CopyIn&&) noexcept;

        void write(std::string_view data);
        void write(std::span<const uint8_t> data);

        // Binary protocol helpers
        void write_int16(std::int16_t value);
        void write_int32(std::int32_t value);
        void write_int64(std::int64_t value);
        void write_float(float value);
        void write_double(double value);
        void write_bool(bool value);
        void write_null();
        void write_text(std::string_view text);
        void write_bytea(std::span<const uint8_t> data);

        void finish();
        void abort(std::string_view error = {});

        [[nodiscard]] std::size_t rows_affected() const {
            return rows_;
        }

    private:
        friend class Connection;
        CopyIn(Connection& conn, PGresult* result);

        Connection* conn_{nullptr};
        std::size_t rows_{0};
        bool finished_{false};
    };

}  // namespace demiplane::db::postgres
