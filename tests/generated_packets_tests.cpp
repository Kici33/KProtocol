// Wave 2: smoke-test the committed generated/ packet catalog.
//
// This test only runs when the generator output is compiled in (the default
// for in-tree builds). It verifies that:
//   - register_generated_packets() succeeds and populates the registry
//   - the auto-generated key constants in kprotocol/generated/packet_keys.hpp
//     line up with real schema entries
//   - the well-known login.serverbound.encryption_begin packet round-trips
//     through the encode/decode pipeline
//   - formerly opaque action-switch packets now round-trip through typed
//     conditional fields
//   - all baseline versions have at least one schema registered

#include "kprotocol/codec.hpp"
#include "kprotocol/generated.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/version.hpp"

#include "kprotocol/generated/packet_keys.hpp"

#include <array>
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

constexpr std::array<kprotocol::ProtocolVersion, 5> kBaselineVersions = {
    kprotocol::ProtocolVersion::v1_8,
    kprotocol::ProtocolVersion::v1_12_2,
    kprotocol::ProtocolVersion::v1_16_5,
    kprotocol::ProtocolVersion::v1_20_4,
    kprotocol::ProtocolVersion::v1_21_1,
};

void test_register_generated_populates_registry() {
    std::cout << "  register_generated_packets() registers >100 packet keys... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);
    KPC_CHECK(registry.size() > 100, "registry size");
    std::cout << "ok (" << registry.size() << " keys)\n";
}

void test_generated_key_constants_resolve() {
    std::cout << "  generated::packet_keys::login_serverbound_encryption_begin resolves... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    const auto key = kprotocol::generated::packet_keys::login_serverbound_encryption_begin;
    const auto* schema = registry.schema_for(key);
    KPC_CHECK(schema != nullptr, "schema");
    KPC_CHECK(schema->state == kprotocol::PacketState::login, "state");
    KPC_CHECK(schema->direction == kprotocol::PacketDirection::serverbound, "direction");
    std::cout << "ok\n";
}

void test_encryption_begin_roundtrip_at_1_20_4() {
    std::cout << "  login.serverbound.encryption_begin round-trips at v1.20.4... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    kprotocol::Packet p;
    p.key = std::string(kprotocol::generated::packet_keys::login_serverbound_encryption_begin);
    p.state = kprotocol::PacketState::login;
    p.direction = kprotocol::PacketDirection::serverbound;
    p.fields["sharedSecret"] = std::vector<std::uint8_t>{0xAA, 0xBB, 0xCC, 0xDD};
    p.fields["verifyToken"]  = std::vector<std::uint8_t>{0x01, 0x02, 0x03};

    const auto encoded = registry.encode_packet(p, kprotocol::ProtocolVersion::v1_20_4);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    KPC_CHECK(consumed == encoded.size(), "full frame consumed");
    const auto decoded = registry.decode_packet(
        frame, kprotocol::ProtocolVersion::v1_20_4,
        kprotocol::PacketState::login, kprotocol::PacketDirection::serverbound);
    KPC_CHECK(std::get<std::vector<std::uint8_t>>(decoded.fields.at("sharedSecret"))
              == std::vector<std::uint8_t>({0xAA, 0xBB, 0xCC, 0xDD}), "sharedSecret");
    KPC_CHECK(std::get<std::vector<std::uint8_t>>(decoded.fields.at("verifyToken"))
              == std::vector<std::uint8_t>({0x01, 0x02, 0x03}), "verifyToken");
    std::cout << "ok\n";
}

void test_every_baseline_version_has_at_least_one_schema() {
    std::cout << "  every baseline version has registered ids... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    // Cross-check a packet that exists at every baseline version. handshake's
    // set_protocol (id 0x00 in handshaking.serverbound) is the canonical
    // "always present" packet.
    for (auto v : kBaselineVersions) {
        const auto* schema = registry.schema_for(
            v, kprotocol::PacketState::handshaking,
            kprotocol::PacketDirection::serverbound, 0x00);
        KPC_CHECK(schema != nullptr, "handshake schema");
        (void)v;
    }
    std::cout << "ok\n";
}

void test_boss_bar_roundtrips_typed_conditional_fields() {
    std::cout << "  boss_bar round-trips typed conditional fields... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    const std::string key = "play.clientbound.boss_bar";
    const auto* schema = registry.schema_for(key);
    KPC_CHECK(schema != nullptr, "boss_bar schema");

    kprotocol::Packet p;
    p.key = key;
    p.state = kprotocol::PacketState::play;
    p.direction = kprotocol::PacketDirection::clientbound;
    p.fields["entityUUID"] = kprotocol::UUID{{{0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
                                               0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10}}};
    p.fields["action"] = std::int32_t{2};
    p.fields["health"] = 0.5F;

    const auto encoded = registry.encode_packet(p, kprotocol::ProtocolVersion::v1_20_4);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(
        frame, kprotocol::ProtocolVersion::v1_20_4,
        kprotocol::PacketState::play, kprotocol::PacketDirection::clientbound);
    KPC_CHECK(std::get<std::int32_t>(decoded.fields.at("action")) == 2, "action");
    KPC_CHECK(std::get<float>(decoded.fields.at("health")) == 0.5F, "health");
    KPC_CHECK(decoded.fields.find("raw") == decoded.fields.end(), "no raw field");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "generated_packets_tests:\n";
    try {
        test_register_generated_populates_registry();
        test_generated_key_constants_resolve();
        test_encryption_begin_roundtrip_at_1_20_4();
        test_every_baseline_version_has_at_least_one_schema();
        test_boss_bar_roundtrips_typed_conditional_fields();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All generated-packet smoke tests passed.\n";
    return 0;
}
