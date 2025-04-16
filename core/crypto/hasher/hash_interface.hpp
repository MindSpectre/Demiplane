#pragma once

#include <string>
#include <vector>

#include "salt_generator.hpp"
namespace demiplane::crypto {
    struct HashInterface {
        virtual ~HashInterface() = default;

        HashInterface() = default;

        virtual std::string hash_function(std::string_view data, std::string_view salt) = 0;

        std::pair<std::string, std::string> hash_with_generated_salt(const std::string_view password) {
            std::string salt = SaltGenerator::generate_base64(16); // 16-byte salt
            std::string hash = hash_function(password, salt);
            return {salt, hash};
        }
    };

} // namespace demiplane::crypto
