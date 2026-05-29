#include "kprotocol/login_security.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <random>
#include <stdexcept>

namespace kprotocol {

S01EncryptionRequestPacket LoginSecurityChallenge::to_packet() const {
    return S01EncryptionRequestPacket{
        .server_id = server_id,
        .public_key = public_key,
        .verify_token = verify_token,
    };
}

std::vector<std::uint8_t> generate_verify_token(const std::size_t bytes) {
    if (bytes == 0U || bytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument("verify token length must be between 1 and INT_MAX bytes");
    }

    std::vector<std::uint8_t> token(bytes);
    std::random_device random;
    std::uniform_int_distribution<int> byte_dist(0, 255);
    std::generate(token.begin(), token.end(), [&] {
        return static_cast<std::uint8_t>(byte_dist(random));
    });
    return token;
}

bool constant_time_equal(
    const std::span<const std::uint8_t> left,
    const std::span<const std::uint8_t> right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }

    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        diff = static_cast<std::uint8_t>(diff | (left[i] ^ right[i]));
    }
    return diff == 0;
}

bool verify_login_token(
    const C01EncryptionResponsePacket& response,
    const std::span<const std::uint8_t> expected_token) noexcept {
    return constant_time_equal(response.verify_token, expected_token);
}

bool is_valid_login_username(const std::string_view username) noexcept {
    if (username.size() < 3U || username.size() > 16U) {
        return false;
    }
    return std::all_of(username.begin(), username.end(), [](const unsigned char c) {
        return std::isalnum(c) != 0 || c == '_';
    });
}

bool send_login_encryption_request(
    const ClientSession& client,
    const LoginSecurityChallenge& challenge) {
    if (!client.valid() || challenge.public_key.empty() || challenge.verify_token.empty()) {
        return false;
    }
    return client.send_packet_direct(challenge.to_packet().to_packet(), client.protocol_version());
}

bool send_login_set_compression(const ClientSession& client, const std::int32_t threshold) {
    if (!client.valid() || threshold < 0) {
        return false;
    }
    if (!client.send_packet_direct(S03SetCompressionPacket{.threshold = threshold}.to_packet(),
                                   client.protocol_version())) {
        return false;
    }
    client.enable_compression(threshold);
    return true;
}

} // namespace kprotocol
