#pragma once

#include <iomanip>
#include <string>

#include "../hash_interface.hpp"

namespace demiplane::crypto {
    class PBKDF2Hash final : HashInterface {
        public:
        PBKDF2Hash()           = default;
        ~PBKDF2Hash() override = default;

        /// @brief Creates a PBKDF2 hash using the given password and salt
        /// @param password The password to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the PBKDF2 hash
        std::string hash_function(std::string_view password, std::string_view salt) override;
    };
}  // namespace demiplane::crypto
