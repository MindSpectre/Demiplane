#pragma once

#include <iomanip>
#include <openssl/hmac.h>
#include <sstream>
#include <string>

#include "hash_creator_interface.hpp"

namespace common::crypto {
    class SHA256Function final : public HashCreator {
    public:
        SHA256Function()           = default;
        ~SHA256Function() override = default;

        /// @brief Creates a 256-bit HMAC hash using the given data and salt
        /// @param data The message to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the HMAC-SHA256 hash
        std::string hash_function(const std::string_view data, const std::string_view salt) override {
            // Combine key and salt to create the HMAC key
            const std::string hmacKey = key + salt.data();

            uint32_t digestLen;

            // Perform HMAC-SHA256
            const unsigned char* digest = HMAC(EVP_sha256(), hmacKey.c_str(), static_cast<int32_t>(hmacKey.size()),
                reinterpret_cast<const unsigned char*>(data.data()), data.length(), nullptr, &digestLen);

            // Convert the digest to a hexadecimal string
            std::ostringstream oss;
            for (uint32_t i = 0; i < digestLen; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int32_t>(digest[i]);
            }

            return oss.str();
        }
    };
} // namespace common::crypto
