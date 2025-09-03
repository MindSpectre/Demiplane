#include <sstream>

#include "pbkdf2.hpp"
#include "sha_256.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

std::string demiplane::crypto::SHA256Hash::hash_function(const std::string_view data, const std::string_view salt) {
    // Combine key and salt to create the HMAC key
    const std::string hmacKey = key_ + salt.data();

    uint32_t digestLen;

    // Perform HMAC-SHA256
    const unsigned char* digest = HMAC(EVP_sha256(),
                                       hmacKey.c_str(),
                                       static_cast<int32_t>(hmacKey.size()),
                                       reinterpret_cast<const unsigned char*>(data.data()),
                                       data.length(),
                                       nullptr,
                                       &digestLen);

    // Convert the digest to a hexadecimal string
    std::ostringstream oss;
    for (uint32_t i = 0; i < digestLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int32_t>(digest[i]);
    }

    return oss.str();
}


std::string demiplane::crypto::PBKDF2Hash::hash_function(const std::string_view password, const std::string_view salt) {
    constexpr int key_length = 32;  // 256-bit hash
    std::vector<unsigned char> derived_key(key_length);

    if (constexpr int iterations = 100000; !PKCS5_PBKDF2_HMAC(password.data(),
                                                              static_cast<int>(password.size()),
                                                              reinterpret_cast<const unsigned char*>(salt.data()),
                                                              static_cast<int>(salt.size()),
                                                              iterations,
                                                              EVP_sha256(),
                                                              key_length,
                                                              derived_key.data())) {
        throw std::runtime_error("Failed to generate PBKDF2 hash");
    }

    // Convert the derived key to a hexadecimal string
    std::ostringstream oss;
    for (const unsigned char c : derived_key) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }

    return oss.str();
}
