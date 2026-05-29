#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/login_security.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

kprotocol::Packet decode_one(
    kprotocol::PacketRegistry& registry,
    const std::vector<std::uint8_t>& encoded,
    const kprotocol::PacketState state,
    const kprotocol::PacketDirection direction) {
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    KPC_CHECK(consumed == encoded.size(), "full frame consumed");
    return registry.decode_packet(frame, kprotocol::ProtocolVersion::v1_21_1, state, direction);
}

void test_encryption_request_roundtrip() {
    std::cout << "  encryption request packet round-trips... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    const kprotocol::LoginSecurityChallenge challenge{
        .server_id = "",
        .public_key = {0x30, 0x81, 0x9F, 0x01},
        .verify_token = {0x10, 0x20, 0x30, 0x40},
    };

    const auto encoded = registry.encode_packet(
        challenge.to_packet().to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    const auto decoded = decode_one(
        registry, encoded, kprotocol::PacketState::login, kprotocol::PacketDirection::clientbound);
    const auto typed = kprotocol::S01EncryptionRequestPacket::from_packet(decoded);
    KPC_CHECK(typed.public_key == challenge.public_key, "public key");
    KPC_CHECK(typed.verify_token == challenge.verify_token, "verify token");
    std::cout << "ok\n";
}

void test_encryption_response_token_verification() {
    std::cout << "  encryption response token verification works... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04};
    const kprotocol::C01EncryptionResponsePacket response{
        .shared_secret = {0xAA, 0xBB, 0xCC},
        .verify_token = expected,
    };

    const auto encoded = registry.encode_packet(
        response.to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    const auto decoded = decode_one(
        registry, encoded, kprotocol::PacketState::login, kprotocol::PacketDirection::serverbound);
    const auto typed = kprotocol::C01EncryptionResponsePacket::from_packet(decoded);
    const std::vector<std::uint8_t> short_token{0x01, 0x02};
    const std::vector<std::uint8_t> wrong_token{0x01, 0x02, 0x03, 0x05};
    KPC_CHECK(kprotocol::verify_login_token(typed, expected), "matching token");
    KPC_CHECK(!kprotocol::verify_login_token(typed, short_token), "short token");
    KPC_CHECK(!kprotocol::verify_login_token(typed, wrong_token), "wrong token");
    std::cout << "ok\n";
}

void test_compression_packet_and_username_validation() {
    std::cout << "  compression packet and username validation work... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    const auto packet = kprotocol::S03SetCompressionPacket{.threshold = 256}.to_packet();
    const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
    const auto decoded = decode_one(
        registry, encoded, kprotocol::PacketState::login, kprotocol::PacketDirection::clientbound);
    KPC_CHECK(kprotocol::S03SetCompressionPacket::from_packet(decoded).threshold == 256, "threshold");

    KPC_CHECK(kprotocol::is_valid_login_username("Player_01"), "valid username");
    KPC_CHECK(!kprotocol::is_valid_login_username("ab"), "too short username");
    KPC_CHECK(!kprotocol::is_valid_login_username("this_name_is_too_long"), "too long username");
    KPC_CHECK(!kprotocol::is_valid_login_username("bad-name"), "invalid username character");

    const auto token = kprotocol::generate_verify_token();
    KPC_CHECK(token.size() == 16U, "default token length");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "login_security_tests:\n";
    try {
        test_encryption_request_roundtrip();
        test_encryption_response_token_verification();
        test_compression_packet_and_username_validation();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All login security tests passed.\n";
    return 0;
}
