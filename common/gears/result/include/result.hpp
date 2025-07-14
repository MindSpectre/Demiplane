#pragma once
#include <exception>
#include <functional>
#include <optional>
#include <utility>

namespace demiplane::gears {
    template <typename T>
    class Result {
    public:
        /* ───── state ────────────────────────────────────────────── */
        [[nodiscard]] bool has_value() const noexcept {
            return value_.has_value();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return has_value();
        }

        [[nodiscard]] bool has_error() const noexcept {
            return static_cast<bool>(error_);
        }

        /* ───── value access (unchecked) ─────────────────────────── */
        const T& value() const & {
            return *value_;
        }

        T& value() & {
            return *value_;
        }

        T&& value() && {
            return std::move(*value_);
        }

        const T& operator*() const & {
            return value();
        }

        const T* operator->() const {
            return &value();
        }

        /* ───── error access / rethrow ───────────────────────────── */
        void rethrow() const {
            if (error_) std::rethrow_exception(error_);
        }

        /* ───── factory helpers (optional) ───────────────────────── */
        template <typename... Args>
        static Result success(Args&&... args)
            requires std::is_constructible_v<T, Args&&...> {
            Result r;
            r.value_.emplace(std::forward<Args>(args)...);
            return r;
        }

        /* ───── multi-catch helper ─────────────────────────────────
           Usage:
               res.capture<A,B,C>([&]{ ... });
        ----------------------------------------------------------------*/
        template <typename... Catch, typename F>
        void capture(F&& f) {
            static_assert(sizeof...(Catch) > 0,
                          "capture<E...>() needs at least one exception type");

            try {
                if constexpr (std::is_void_v<std::invoke_result_t<F>>) std::forward<F>(f)();
                else value_ = std::forward<F>(f)();
            }
            catch (...) {
                // Use fold expression to generate individual catch blocks at compile time
                if (!try_catch_specific<Catch...>(std::current_exception())) {
                    // If none of the specified exceptions matched, rethrow
                    throw;
                }
            }
        }

    private:
        // Compile-time expansion of catch blocks using fold expressions
        template <typename First, typename... Rest>
        bool try_catch_specific(std::exception_ptr eptr) {
            try {
                std::rethrow_exception(eptr);
            }
            catch (const First& e) {
                error_ = std::make_exception_ptr(e);
                return true;
            }
            catch (...) {
                if constexpr (sizeof...(Rest) > 0) {
                    return try_catch_specific<Rest...>(eptr);
                }
                return false;
            }
        }

        std::optional<T>   value_;
        std::exception_ptr error_;
    };

    template <>
    class Result<void> {
    public:
        [[nodiscard]] bool has_value() const noexcept {
            return !error_;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return has_value();
        }

        [[nodiscard]] bool has_error() const noexcept {
            return static_cast<bool>(error_);
        }

        void rethrow() const {
            if (error_) std::rethrow_exception(error_);
        }

        template <typename... Catch, typename Function>
        void capture(Function&& f) {
            static_assert(sizeof...(Catch) > 0, "capture<E...>() needs at least one exception type");
            try {
                std::forward<Function>(f)();
            }
            catch (...) {
                if (!try_catch_specific<Catch...>(std::current_exception())) {
                    throw;
                }
            }
        }

    private:
        template <typename First, typename... Rest>
        bool try_catch_specific(std::exception_ptr eptr) {
            try {
                std::rethrow_exception(eptr);
            }
            catch (const First& e) {
                error_ = std::make_exception_ptr(e);
                return true;
            }
            catch (...) {
                if constexpr (sizeof...(Rest) > 0) {
                    return try_catch_specific<Rest...>(eptr);
                }
                return false;
            }
        }

        std::exception_ptr error_;
    };
} // namespace demiplane::gears
