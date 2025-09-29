#pragma once
#include <chrono>
#include <string>

#include <boost/exception/all.hpp>
#include <boost/stacktrace/stacktrace.hpp>
#include <boost/system/error_code.hpp>
namespace demiplane::db {

    // ============== Error Info Tags ==============
    // These tags attach additional context to exceptions
    typedef boost::error_info<struct tag_error_code, int> error_code_info;
    typedef boost::error_info<struct tag_sqlstate, std::string> sqlstate_info;
    typedef boost::error_info<struct tag_query, std::string> query_info;
    typedef boost::error_info<struct tag_database_name, std::string> database_info;
    typedef boost::error_info<struct tag_host, std::string> host_info;
    typedef boost::error_info<struct tag_port, int> port_info;
    typedef boost::error_info<struct tag_table_name, std::string> table_info;
    typedef boost::error_info<struct tag_column_name, std::string> column_info;
    typedef boost::error_info<struct tag_constraint_name, std::string> constraint_info;
    typedef boost::error_info<struct tag_retry_after, std::chrono::milliseconds> retry_after_info;
    typedef boost::error_info<struct tag_affected_rows, std::size_t> affected_rows_info;
    typedef boost::error_info<struct tag_error_position, std::size_t> error_position_info;
    typedef boost::error_info<struct tag_severity, std::string> severity_info;
    typedef boost::error_info<struct tag_transaction_id, std::string> transaction_id_info;

    // ============== Base Exception ==============
    class database_exception : public boost::exception, public std::exception {
    protected:
        mutable std::string cached_what;

    public:
        database_exception() noexcept  = default;
        ~database_exception() override = default;

        const char* what() const noexcept override {
            if (cached_what.empty()) {
                cached_what = boost::diagnostic_information(*this, false);
            }
            return cached_what.c_str();
        }

        // Helper to check if retryable
        virtual bool is_retryable() const noexcept {
            return false;
        }

        // Get error category for logging/monitoring
        [[nodiscard]] constexpr virtual const char* category() const noexcept = 0;
    };

    // ============== Client Side Exceptions ==============
    class client_error : public database_exception {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client";
        }
    };

    // Invalid input/arguments
    class invalid_argument_error : public client_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument";
        }
    };

    class syntax_error : public invalid_argument_error {
    public:
        syntax_error() = default;
        explicit syntax_error(const std::string& msg) {
            *this << boost::error_info<struct tag_message, std::string>(msg);
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.syntax";
        }
    };

    class invalid_parameter_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.parameter";
        }
    };

    class type_mismatch_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.type";
        }
    };

    class null_conversion_error : public invalid_argument_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.invalid_argument.null_conversion";
        }
    };

    // Configuration/setup errors
    class configuration_error : public client_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration";
        }
    };

    class connection_string_error : public configuration_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration.connection_string";
        }
    };

    class authentication_error : public configuration_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "client.configuration.auth";
        }
    };

    // ============== Server Side Exceptions ==============
    class server_error : public database_exception {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server";
        }
    };

    // Runtime/transient errors
    class runtime_error : public server_error {
    public:
        bool is_retryable() const noexcept override {
            return true;
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime";
        }
    };

    class connection_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection";
        }
    };

    class connection_lost_error : public connection_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection.lost";
        }
    };

    class connection_timeout_error : public connection_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.connection.timeout";
        }
    };

    class deadlock_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.deadlock";
        }
    };

    class lock_timeout_error : public runtime_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.runtime.lock_timeout";
        }
    };

    // Constraint violations
    class constraint_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint";
        }
    };

    class unique_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.unique";
        }
    };

    class foreign_key_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.foreign_key";
        }
    };

    class check_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.check";
        }
    };

    class not_null_violation_error : public constraint_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.constraint.not_null";
        }
    };

    // Data errors
    class data_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data";
        }
    };

    class data_too_long_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.too_long";
        }
    };

    class numeric_overflow_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.overflow";
        }
    };

    class division_by_zero_error : public data_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.data.division_by_zero";
        }
    };

    // Access/permission errors
    class access_error : public server_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access";
        }
    };

    class permission_denied_error : public access_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access.permission";
        }
    };

    class object_not_found_error : public access_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.access.not_found";
        }
    };

    // Resource errors
    class resource_error : public server_error {
    public:
        bool is_retryable() const noexcept override {
            return true;
        }
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource";
        }
    };

    class out_of_memory_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.memory";
        }
    };

    class disk_full_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.disk";
        }
    };

    class too_many_connections_error : public resource_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "server.resource.connections";
        }
    };

    // ============== Fatal Exceptions (with stack traces) ==============
    class fatal_error : public database_exception {
    public:
        fatal_error() {
            stack_trace = boost::stacktrace::to_string(boost::stacktrace::stacktrace());
        }

        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal";
        }

        const std::string& get_stack_trace() const noexcept {
            return stack_trace;
        }

    private:
        std::string stack_trace;
    };

    class internal_error : public fatal_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal.internal";
        }
    };

    class corruption_error : public fatal_error {
    public:
        [[nodiscard]] constexpr const char* category() const noexcept override {
            return "fatal.corruption";
        }
    };

}  // namespace demiplane::db
