#pragma once
#include <openssl/evp.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace kprotocol::crypto {
using Bytes = std::vector<std::uint8_t>;
std::string server_hash(std::string_view id, std::span<const std::uint8_t> secret, std::span<const std::uint8_t> public_key);
Bytes random_secret();
Bytes rsa_encrypt(std::span<const std::uint8_t> public_key, std::span<const std::uint8_t> value);
class Cipher {
public:
    Cipher(std::span<const std::uint8_t> secret, bool encrypt);
    Bytes update(std::span<const std::uint8_t> input);
private:
    std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context_{nullptr, EVP_CIPHER_CTX_free};
};
}
