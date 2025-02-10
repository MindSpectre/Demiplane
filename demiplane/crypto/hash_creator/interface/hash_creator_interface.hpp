#pragma once

#include <iomanip>
#include <openssl/rand.h>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace common::crypto {
    class HashCreator {
    protected:
        /// @brief Secret key for HMAC
        std::string key;
        /// @brief Generates a random salt
        /// @param length The length of the salt in bytes
        /// @return A random salt as a hexadecimal string
        static std::string generate_salt(size_t length) {
            std::vector<unsigned char> salt(length);
            if (!RAND_bytes(salt.data(), static_cast<int>(length))) {
                throw std::runtime_error("Failed to generate random salt");
            }

            std::ostringstream oss;
            for (const unsigned char c : salt) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
            }
            return oss.str();
        }

    public:
        virtual ~HashCreator() = default;

        HashCreator() = default;

        virtual std::string hash_function(std::string_view data, std::string_view salt) = 0;

        std::pair<std::string, std::string> hash_with_generated_salt(const std::string& password) {
            std::string salt = generate_salt(16); // 16-byte salt
            std::string hash = hash_function(password, salt);
            return {salt, hash};
        }

        /// @brief Sets the secret key for HMAC
        void set_key(const std::string& newKey) {
            key = newKey;
        }
    };
} // namespace common::crypto
