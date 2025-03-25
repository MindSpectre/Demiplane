#pragma once

#include <exception>
#include <functional>
#include <string>
namespace demiplane {

    enum class Status {
        Success,
        NonCriticalError, // Operation failed but can be retried
        CriticalError, // Irrecoverable failure
        UndefinedNonCriticalError, // Operation failed but can be retried
        UndefinedCriticalError // Irrecoverable failure
    };
    template <typename ResultResp = void>
    class IRes {
    public:
        void capture(const std::function<void()>& func, const std::function<void()>& if_fall = {}) {
            try {
                func();
            } catch (const std::exception& e) {
                exception_ = std::make_exception_ptr(e);
                status_    = Status::NonCriticalError;
                message_   = e.what();
                if_fall();
            } catch (...) {
                exception_ = std::current_exception();
                status_    = Status::UndefinedNonCriticalError;
                if_fall();
            }
        }
        void critical_zone(const std::function<void()>& func, const std::function<void()>& if_fall = {}) {
            try {
                func();
            } catch (const std::exception& e) {
                exception_ = std::make_exception_ptr(e);
                status_    = Status::CriticalError;
                message_   = e.what();
                if_fall();
            }
            catch (...) {
                exception_ = std::current_exception();
                status_    = Status::UndefinedCriticalError;
                if_fall();
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
        // Only available if ResultResp is not void
        template <typename R>
            requires (!std::same_as<ResultResp, void>)
        void set(R&& resp) {
            response_ = std::forward<R>(resp);
        }

        ResultResp response()
            requires (!std::same_as<ResultResp, void>)
        {
            return std::move(response_);
        }
        template <typename R = ResultResp>
            requires (!std::same_as<R, void>)
        explicit IRes(R&& response) : response_(std::forward<R>(response)) {}
        IRes() = default;


        template <typename X>
        explicit IRes(const IRes<X>& other)
            : message_{other.message_}, status_{other.status_}, exception_{other.exception_} {}
        template <typename X>
        IRes& operator=(const IRes<X>& other) {
            if (this == &other) {
                return *this;
            }
            message_   = other.message_;
            status_    = other.status_;
            exception_ = other.exception_;
            return *this;
        }
        static IRes sOk() {
            return IRes{};
        }

    private:
        std::conditional_t<std::same_as<ResultResp, void>, char, ResultResp> response_;
        std::string message_;
        Status status_{Status::Success};
        std::exception_ptr exception_;
    };

} // namespace demiplane
