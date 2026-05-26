// Version range coverage: wire 47 (1.8) through latest catalog anchor (1.21.11 / 774),
// plus forward-compat fallback for unknown future wires until the catalog is regenerated.

#include "kprotocol/codec.hpp"
#include "kprotocol/generated.hpp"
#include "kprotocol/generated/packet_keys.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/version.hpp"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

void test_catalog_anchor_resolution() {
    std::cout << "  catalog_anchor_for resolves 47 and 774... ";
    KPC_CHECK(kprotocol::catalog_anchor_for(47) == kprotocol::ProtocolVersion::v1_8,
              "anchor 47");
    KPC_CHECK(kprotocol::protocol_number(kprotocol::latest_catalog_version()) == 774,
              "latest wire");
    KPC_CHECK(kprotocol::catalog_anchor_for(774) == kprotocol::ProtocolVersion::v1_21_11,
              "anchor 774");
    KPC_CHECK(kprotocol::catalog_anchor_for(775) == kprotocol::ProtocolVersion::v1_21_11,
              "future wire falls back to latest anchor");
    std::cout << "ok\n";
}

void test_keep_alive_roundtrip_at_endpoints() {
    std::cout << "  keep_alive round-trips at v1_8 and v1_21_11... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    const auto key = kprotocol::generated::packet_keys::play_clientbound_keep_alive;
    for (const auto version :
         {kprotocol::ProtocolVersion::v1_8, kprotocol::ProtocolVersion::v1_21_11}) {
        kprotocol::Packet p;
        p.key = std::string(key);
        p.state = kprotocol::PacketState::play;
        p.direction = kprotocol::PacketDirection::clientbound;
        if (version == kprotocol::ProtocolVersion::v1_8) {
            p.fields["keepAliveId"] = std::int32_t{0x12345678};
        } else {
            p.fields["keepAliveId"] = std::int64_t{0x123456789ABCDEF0LL};
        }

        const auto encoded = registry.encode_packet(p, version);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame),
                  "decode frame");
        const auto decoded = registry.decode_packet(
            frame, version, kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound);
        if (version == kprotocol::ProtocolVersion::v1_8) {
            KPC_CHECK(std::get<std::int32_t>(decoded.fields.at("keepAliveId")) == 0x12345678,
                      "payload");
        } else {
            KPC_CHECK(std::get<std::int64_t>(decoded.fields.at("keepAliveId")) ==
                          0x123456789ABCDEF0LL,
                      "payload");
        }
    }
    std::cout << "ok\n";
}

void test_future_wire_uses_nearest_catalog_ids() {
    std::cout << "  unknown wire 775 inherits v1_21_11 packet ids... ";
    kprotocol::PacketRegistry registry;
    kprotocol::register_generated_packets(registry);

    const auto key = kprotocol::generated::packet_keys::play_clientbound_keep_alive;
    const auto at_latest = registry.packet_id_for(key, kprotocol::ProtocolVersion::v1_21_11);
    const auto at_future = registry.packet_id_for(
        key, static_cast<kprotocol::ProtocolVersion>(775));
    KPC_CHECK(at_latest.has_value() && at_future.has_value(), "ids");
    KPC_CHECK(*at_latest == *at_future, "future id matches latest anchor");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "version_range_tests:\n";
    test_catalog_anchor_resolution();
    test_keep_alive_roundtrip_at_endpoints();
    test_future_wire_uses_nearest_catalog_ids();
    std::cout << "All version range tests passed.\n";
    return 0;
}
