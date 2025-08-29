#pragma once

#include <cstdint>
#include <string>
#include <vector>
namespace demiplane::crypto {
    class SaltGenerator {
    public:
        /// @brief Generate a cryptographically secure random salt of the specified size.
        /// @return The raw bytes in a std::vector<uint8_t>.
        [[nodiscard]] static std::vector<std::uint8_t> generate_bytes(std::size_t size);

        /// @brief Generate salt and return it as a hex string (e.g., "3afc18...").
        [[nodiscard]] static std::string generate_hex(std::size_t size);

        /// @brief Generate salt and return it as a Base64-encoded string.
        [[nodiscard]] static std::string generate_base64(std::size_t size);

    private:
        /// Helper function: Encode data as Base64 using OpenSSL’s BIO and EVP APIs.
        [[nodiscard]] static std::string encode_base64(const std::vector<std::uint8_t>& data);
    };
}  // namespace demiplane::crypto
