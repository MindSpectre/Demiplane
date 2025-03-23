#pragma once

#include <exception>
#include <functional>
#include <string>
namespace demiplane {

    enum class Status {
        Success,
        NonCriticalError, // Operation failed but can be retried
        CriticalError // Irrecoverable failure
    };
    class IRes {
    public:
        void capture(const std::function<void()>& func) {
            try {
                func();
            } catch (...) {
                exception_ = std::current_exception();
                status_    = Status::NonCriticalError;
            }
        }
        void critical_zone(const std::function<void()>& func) {
            try {
                func();
            } catch (...) {
                exception_ = std::current_exception();
                status_    = Status::CriticalError;
            }
        }
        void rethrow() const {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
        }

        [[nodiscard]] bool has_captured() const {
            return static_cast<bool>(exception_);
        }

        [[nodiscard]] bool is_ok() const {
            return status_ == Status::Success;
        }
        [[nodiscard]] bool is_err() const {
            return status_ == Status::NonCriticalError || status_ == Status::CriticalError;
        }
        [[nodiscard]] const std::string& message() const {
            return message_;
        }
        void set_message(std::string message) {
            message_ = std::move(message);
        }
        [[nodiscard]] Status status() const {
            return status_;
        }
        void set_status(const Status status) {
            status_ = status;
        }

    private:
        std::string message_;
        Status status_{Status::Success};
        std::exception_ptr exception_;
    };

    extern IRes sOk;
} // namespace demiplane
