#include "kprotocol/crypto.hpp"
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <array>
#include <stdexcept>
namespace kprotocol::crypto {
std::string server_hash(std::string_view id, std::span<const std::uint8_t> secret, std::span<const std::uint8_t> key) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::array<unsigned char, 20> digest{};
    unsigned int size = 0;
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha1(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), id.data(), id.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), secret.data(), secret.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), key.data(), key.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest.data(), &size) != 1) throw std::runtime_error("SHA-1 failed");
    const bool negative = (digest[0] & 0x80) != 0;
    if (negative) {
        unsigned carry = 1;
        for (auto i = digest.rbegin(); i != digest.rend(); ++i) {
            const unsigned value = static_cast<unsigned char>(~*i) + carry;
            *i = static_cast<unsigned char>(value);
            carry = value >> 8;
        }
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    const auto first = result.find_first_not_of('0');
    result = first == std::string::npos ? "0" : result.substr(first);
    return negative ? "-" + result : result;
}
Bytes random_secret() {
    Bytes result(16);
    if (RAND_bytes(result.data(), static_cast<int>(result.size())) != 1) throw std::runtime_error("Secure random generation failed");
    return result;
}
Bytes rsa_encrypt(std::span<const std::uint8_t> public_key, std::span<const std::uint8_t> value) {
    const auto* cursor = public_key.data();
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        d2i_PUBKEY(nullptr, &cursor, static_cast<long>(public_key.size())), EVP_PKEY_free);
    if (!key || EVP_PKEY_base_id(key.get()) != EVP_PKEY_RSA || cursor != public_key.data() + public_key.size())
        throw std::runtime_error("Server supplied an invalid RSA public key");
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> ctx(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
    std::size_t size = 0;
    if (!ctx || EVP_PKEY_encrypt_init(ctx.get()) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx.get(), RSA_PKCS1_PADDING) <= 0 ||
        EVP_PKEY_encrypt(ctx.get(), nullptr, &size, value.data(), value.size()) <= 0) throw std::runtime_error("RSA encryption failed");
    Bytes out(size);
    if (EVP_PKEY_encrypt(ctx.get(), out.data(), &size, value.data(), value.size()) <= 0) throw std::runtime_error("RSA encryption failed");
    out.resize(size);
    return out;
}
Cipher::Cipher(std::span<const std::uint8_t> secret, bool encrypt) : context_(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free) {
    if (secret.size() != 16 || !context_ || EVP_CipherInit_ex(context_.get(), EVP_aes_128_cfb8(), nullptr,
            secret.data(), secret.data(), encrypt ? 1 : 0) != 1) throw std::runtime_error("AES initialization failed");
}
Bytes Cipher::update(std::span<const std::uint8_t> input) {
    Bytes out(input.size() + 16);
    int count = 0;
    if (EVP_CipherUpdate(context_.get(), out.data(), &count, input.data(), static_cast<int>(input.size())) != 1)
        throw std::runtime_error("AES stream failed");
    out.resize(static_cast<std::size_t>(count));
    return out;
}
}
