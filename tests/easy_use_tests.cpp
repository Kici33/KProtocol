// Easy-to-use API smoke tests: ProtocolRuntime, typed UI packets, block-by-name helper.

#include "kprotocol/kprotocol.hpp"

#include "kprotocol/generated/packet_keys.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

void roundtrip_packet(
    kprotocol::PacketRegistry& registry,
    kprotocol::Packet packet,
    kprotocol::ProtocolVersion version) {

    const auto encoded = registry.encode_packet(packet, version);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(
        frame, version, packet.state, packet.direction);
    KPC_CHECK(decoded.key == packet.key, "key");
}

void test_protocol_runtime() {
    std::cout << "  ProtocolRuntime auto-init... ";
    kprotocol::ProtocolRuntime runtime;
    KPC_CHECK(runtime.registry.size() > 200, "registry populated");
    KPC_CHECK(
        runtime.registry.packet_id_for(
            kprotocol::generated::packet_keys::play_clientbound_set_title_text,
            kprotocol::ProtocolVersion::v1_21_1)
            .has_value(),
        "catalog lookup");
    std::cout << "ok\n";
}

void test_typed_ui_packets_1_17_and_1_21() {
    std::cout << "  typed UI packets wire 755 and 767... ";
    kprotocol::ProtocolRuntime runtime;

    kprotocol::S60SetTitleTextPacket title{.text = "Hello"};
    roundtrip_packet(
        runtime.registry,
        title.to_packet(kprotocol::ProtocolVersion::v1_17),
        kprotocol::ProtocolVersion::v1_17);
    roundtrip_packet(
        runtime.registry,
        title.to_packet(kprotocol::ProtocolVersion::v1_21_1),
        kprotocol::ProtocolVersion::v1_21_1);

    kprotocol::S55ActionBarPacket bar{.text = "Action"};
    roundtrip_packet(
        runtime.registry,
        bar.to_packet(kprotocol::ProtocolVersion::v1_21_1),
        kprotocol::ProtocolVersion::v1_21_1);

    kprotocol::S62SetTitleTimePacket times{.fade_in = 10, .stay = 70, .fade_out = 20};
    roundtrip_packet(
        runtime.registry,
        times.to_packet(),
        kprotocol::ProtocolVersion::v1_21_1);

    kprotocol::S3DScoreboardDisplayPacket display{
        .position = 1,
        .name = "sidebar",
    };
    roundtrip_packet(
        runtime.registry,
        display.to_packet(kprotocol::ProtocolVersion::v1_8),
        kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(
        runtime.registry,
        display.to_packet(kprotocol::ProtocolVersion::v1_21_1),
        kprotocol::ProtocolVersion::v1_21_1);

    const auto objective = kprotocol::S3BScoreboardObjectivePacket::make_create("obj", "Demo");
    roundtrip_packet(runtime.registry, objective.to_packet(), kprotocol::ProtocolVersion::v1_21_5);

    const auto score = kprotocol::S3CScoreboardScorePacket::make_set("Player", "obj", 7);
    roundtrip_packet(
        runtime.registry,
        score.to_packet(kprotocol::ProtocolVersion::v1_21_5),
        kprotocol::ProtocolVersion::v1_21_5);

    kprotocol::S10ClearTitlesPacket clear{.reset_times = false};
    roundtrip_packet(
        runtime.registry,
        clear.to_packet(),
        kprotocol::ProtocolVersion::v1_21_5);

    std::cout << "ok\n";
}

void test_block_registry_lookup() {
    std::cout << "  BlockRegistry default_state_id... ";
    const auto stone = kprotocol::BlockRegistry::default_state_id(
        kprotocol::ProtocolVersion::v1_21_1, "stone");
    KPC_CHECK(stone.has_value(), "stone exists");
    std::cout << "ok\n";
}

} // namespace

int main() {
    try {
        std::cout << "easy_use_tests:\n";
        test_protocol_runtime();
        test_typed_ui_packets_1_17_and_1_21();
        test_block_registry_lookup();
        std::cout << "all easy_use_tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "easy_use_tests failed: " << ex.what() << '\n';
        return 1;
    }
}
