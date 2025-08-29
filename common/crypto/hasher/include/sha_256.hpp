#pragma once

#include <string>

#include "../hash_interface.hpp"

namespace demiplane::crypto {
    class SHA256Hash final : HashInterface {
        public:
        SHA256Hash()           = default;
        ~SHA256Hash() override = default;

        /// @brief Creates a 256-bit HMAC hash using the given data and salt
        /// @param data The message to hash
        /// @param salt The salt to use in the hash
        /// @return A hexadecimal string representing the HMAC-SHA256 hash
        std::string hash_function(std::string_view data, std::string_view salt) override;
        [[nodiscard]] const std::string& get_key() const {
            return key_;
        }
        void set_key(std::string key) {
            this->key_ = std::move(key);
        }

        private:
        std::string key_;
    };
}  // namespace demiplane::crypto
