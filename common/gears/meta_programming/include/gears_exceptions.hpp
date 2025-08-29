#pragma once
#include <exception>

namespace demiplane::gears {
    /**
     * Attempts to catch an exception of a specific type or one of multiple specific types from
     * the provided exception pointer and returns if a match was found.
     *
     * This method checks if the exception pointed to by the exception pointer matches the type
     * of the first template parameter. If no match is found, it recursively checks other
     * specified types in the parameter pack.
     *
     * @tparam First The first exception type to check against.
     * @tparam Rest Additional exception types to check against.
     * @param e_ptr A std::exception_ptr pointing to the exception to be checked.
     * @return True if the exception matches any specified type, otherwise false.
     */
    template <typename First, typename... Rest>
    bool try_catch_specific(const std::exception_ptr e_ptr) {
        try {
            std::rethrow_exception(e_ptr);
        } catch (const First& e) {
            return true;
        } catch (...) {
            if constexpr (sizeof...(Rest) > 0) {
                return try_catch_specific<Rest...>(e_ptr);
            }
            return false;
        }
    }
}  // namespace demiplane::gears
