#pragma once

#include <cstddef> // for std::size_t
#include <cstdint> // for std::uint8_t
#include <openssl/bio.h> // for BIO_* base64
#include <openssl/buffer.h>
#include <openssl/evp.h> // for EVP_*
#include <openssl/rand.h> // for RAND_bytes
#include <stdexcept> // for std::runtime_error
#include <string>
#include <vector>

namespace common::crypto {
    class SaltGenerator {
    public:
        /// Generate a cryptographically secure random salt of the specified size.
        /// Returns the raw bytes in a std::vector<uint8_t>.
        [[nodiscard]] static std::vector<std::uint8_t> generate_bytes(const std::size_t size) {
            if (size == 0) {
                throw std::runtime_error("SaltGenerator::generateBytes: size must be greater than 0");
            }

            std::vector<std::uint8_t> salt(size);
            // RAND_bytes returns 1 on success, 0 or -1 on failure.
            if (RAND_bytes(salt.data(), static_cast<int32_t>(salt.size())) != 1) {
                throw std::runtime_error("SaltGenerator::generateBytes: RAND_bytes failed");
            }

            return salt;
        }

        /// Generate salt and return it as a hex string (e.g., "3afc18...").
        [[nodiscard]] static std::string generate_hex(const std::size_t size) {
            auto saltBytes = generate_bytes(size);

            // Each byte becomes two hex characters.
            std::string hex_string;
            hex_string.reserve(size * 2);

            static constexpr std::string_view hex_map = "0123456789abcdef";
            for (const auto byte : saltBytes) {
                hex_string.push_back(hex_map[(byte & 0xF0) >> 4]);
                hex_string.push_back(hex_map[byte & 0x0F]);
            }

            return hex_string;
        }

        /// Generate salt and return it as a Base64-encoded string.
        [[nodiscard]] static std::string generate_base64(const std::size_t size) {
            const auto salt_bytes = generate_bytes(size);
            return encode_base64(salt_bytes);
        }

    private:
        /// Helper function: Encode data as Base64 using OpenSSL’s BIO and EVP APIs.
        [[nodiscard]] static std::string encode_base64(const std::vector<std::uint8_t>& data) {
            if (data.empty()) {
                return {};
            }

            // Create a BIO chain with a Base64 filter.
            BIO* bio = BIO_new(BIO_s_mem());
            if (!bio) {
                throw std::runtime_error("SaltGenerator::encodeBase64: Failed to create BIO");
            }

            BIO* b64 = BIO_new(BIO_f_base64());
            if (!b64) {
                BIO_free(bio);
                throw std::runtime_error("SaltGenerator::encodeBase64: Failed to create Base64 BIO");
            }

            // By default, Base64-encoding includes newlines every 64 characters.
            // Disable newlines for a continuous Base64 output.
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

            // Chain them: base64 filter on top of the memory sink.
            bio = BIO_push(b64, bio);

            // Write the data to the BIO. This encodes the data in Base64.
            if (BIO_write(bio, data.data(), static_cast<int>(data.size())) <= 0) {
                BIO_free_all(bio);
                throw std::runtime_error("SaltGenerator::encodeBase64: BIO_write failed");
            }

            // Flush the BIO to ensure all data is written.
            if (BIO_flush(bio) < 1) {
                BIO_free_all(bio);
                throw std::runtime_error("SaltGenerator::encodeBase64: BIO_flush failed");
            }

            // Get a pointer to the encoded data.
            BUF_MEM* buffer_ptr = nullptr;
            BIO_get_mem_ptr(bio, &buffer_ptr);
            if (!buffer_ptr || !buffer_ptr->data || buffer_ptr->length == 0) {
                BIO_free_all(bio);
                throw std::runtime_error("SaltGenerator::encodeBase64: Failed to retrieve encoded data");
            }

            // Copy out the encoded data into a std::string.
            std::string encoded(buffer_ptr->data, buffer_ptr->length);

            // Clean up.
            BIO_free_all(bio);

            return encoded;
        }
    };
}
