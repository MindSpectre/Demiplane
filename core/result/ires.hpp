#pragma once

#include <exception>
#include <functional>
#include <iostream>
namespace demiplane {

    enum class Status { Success, NonCriticalError, CriticalError };

    class Result {
    public:
        Result() = default;
        explicit Result(const Status status) : status_(status) {}

        void capture(const std::function<void()>& func,
            const std::function<std::exception_ptr(const std::exception& e)>& fallback = DefaultFallback()) {
            try {
                func();
            } catch (const std::exception& e) {
                status_    = Status::NonCriticalError;
                exception_ = fallback(e);
            }
        }
        explicit operator bool() const {
            return is_ok();
        }
        void critical_zone(const std::function<void()>& func,
            const std::function<std::exception_ptr(const std::exception& e)>& fallback= DefaultFallback()) {
            try {
                func();
            } catch (const std::exception& e) {
                std::cerr << e.what() << std::endl;
                status_    = Status::CriticalError;
                exception_ = fallback(e);
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

        [[nodiscard]] Status status() const {
            return status_;
        }
        static Result sOk() {
            return Result{Status::Success};
        }
        static std::function<std::exception_ptr(const std::exception& e)> DefaultFallback() {
            return [](const std::exception& e) { return std::make_exception_ptr(e); };
        }

    protected:
        Status status_{Status::Success};
        std::exception_ptr exception_;
    };

    template <typename T>
    class Interceptor : public Result {
    public:
        Interceptor() = default;
        Interceptor(const Status status, T&& value) : Result(status), response_(std::move(value)) {}
        explicit Interceptor(T&& value) : response_(std::forward<T>(value)) {}


        template <typename U>
        explicit Interceptor(const Interceptor<U>& other) : Result(other), response_(other.response_) {}

        template <typename U>
        Interceptor& operator=(const Interceptor<U>& other) {
            if (this != &other) {
                Result::operator=(other);
                response_ = other.response_;
            }
            return *this;
        }
        const T& operator *() const {
            return response_;
        }
        const T* operator->() const {
            return &response_;
        }

        T* operator->() {
            return &response_;
        }
        void set(T&& value) {
            response_ = std::forward<T>(value);
        }
        void set(const T& value) {
            response_ = value;
        }

        T& ref() {
            return response_;
        }

        T&& response() {
            return std::move(response_);
        }

        static Interceptor sOk()
            requires std::is_default_constructible_v<T>
        {
            return Interceptor{Status::Success, {}};
        }

    private:
        T response_;
    };

    class ResultContext {
    public:
        std::function<std::exception_ptr(const std::exception& caught_exception)> converter;
    };
} // namespace demiplane
