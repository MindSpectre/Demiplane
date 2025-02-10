#pragma once

#include <iomanip>
#include <sstream>
#include <string>

#include "hash_creator_interface.hpp"

namespace common::crypto {
    class PBKDF2Hash final : public HashCreator {
    public:
        PBKDF2Hash()           = default;
        ~PBKDF2Hash() override = default;

        /// @brief Creates a PBKDF2 hash using the given password and salt
        /// @param password The password to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the PBKDF2 hash
        std::string hash_function(const std::string_view password, const std::string_view salt) override {
            constexpr int key_length = 32; // 256-bit hash
            std::vector<unsigned char> derived_key(key_length);

            if (constexpr int iterations = 100000; !PKCS5_PBKDF2_HMAC(password.data(),
                    static_cast<int>(password.size()), reinterpret_cast<const unsigned char*>(salt.data()),
                    static_cast<int>(salt.size()), iterations, EVP_sha256(), key_length, derived_key.data())) {
                throw std::runtime_error("Failed to generate PBKDF2 hash");
            }

            // Convert the derived key to a hexadecimal string
            std::ostringstream oss;
            for (const unsigned char c : derived_key) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            }

            return oss.str();
        }
    };
} // namespace common::crypto
