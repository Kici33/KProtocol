#pragma once

#include "kprotocol/packets/login/C01EncryptionResponsePacket.hpp"
#include "kprotocol/packets/login/S01EncryptionRequestPacket.hpp"
#include "kprotocol/packets/login/S03SetCompressionPacket.hpp"
#include "kprotocol/server.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kprotocol {

struct LoginSecurityChallenge {
    std::string server_id;
    std::vector<std::uint8_t> public_key;
    std::vector<std::uint8_t> verify_token;

    [[nodiscard]] S01EncryptionRequestPacket to_packet() const;
};

[[nodiscard]] std::vector<std::uint8_t> generate_verify_token(std::size_t bytes = 16);
[[nodiscard]] bool constant_time_equal(std::span<const std::uint8_t> left,
                                       std::span<const std::uint8_t> right) noexcept;
[[nodiscard]] bool verify_login_token(const C01EncryptionResponsePacket& response,
                                      std::span<const std::uint8_t> expected_token) noexcept;
[[nodiscard]] bool is_valid_login_username(std::string_view username) noexcept;

bool send_login_encryption_request(const ClientSession& client,
                                   const LoginSecurityChallenge& challenge);
bool send_login_set_compression(const ClientSession& client, std::int32_t threshold);

} // namespace kprotocol
