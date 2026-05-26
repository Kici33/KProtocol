// Title, action bar, and scoreboard encode/decode across legacy and modern wires.

#include "kprotocol/codec.hpp"
#include "kprotocol/initialize.hpp"
#include "kprotocol/packets/play/S10ClearTitlesPacket.hpp"
#include "kprotocol/packets/play/S45TitlePacket.hpp"
#include "kprotocol/packets/play/S55ActionBarPacket.hpp"
#include "kprotocol/packets/play/S60SetTitleTextPacket.hpp"
#include "kprotocol/packets/play/S61SetTitleSubtitlePacket.hpp"
#include "kprotocol/packets/play/S62SetTitleTimePacket.hpp"
#include "kprotocol/packets/play/S3BScoreboardObjectivePacket.hpp"
#include "kprotocol/packets/play/S3CScoreboardScorePacket.hpp"
#include "kprotocol/packets/play/S3DScoreboardDisplayPacket.hpp"
#include "kprotocol/text_component.hpp"

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

void roundtrip(
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

void test_legacy_title_action_bar_scoreboard_1_8() {
    std::cout << "  legacy title/action bar/scoreboard wire 47... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S45TitlePacket title;
    title.action = 0;
    title.tail = [] {
        std::vector<std::uint8_t> out;
        kprotocol::codec::write_string(out, kprotocol::json_text("Hello"));
        return out;
    }();
    roundtrip(registry, title.to_packet(), kprotocol::ProtocolVersion::v1_8);

    kprotocol::Packet chat{
        .key = std::string(kprotocol::generated::packet_keys::play_clientbound_chat),
        .state = kprotocol::PacketState::play,
        .direction = kprotocol::PacketDirection::clientbound,
        .fields = {
            {"message", kprotocol::json_text("Action")},
            {"position", static_cast<std::int8_t>(2)},
        },
    };
    roundtrip(registry, chat, kprotocol::ProtocolVersion::v1_8);

    const auto objective = kprotocol::S3BScoreboardObjectivePacket::make_create("obj", "Demo");
    roundtrip(registry, objective.to_packet(), kprotocol::ProtocolVersion::v1_8);

    const auto score = kprotocol::S3CScoreboardScorePacket::make_set("Player", "obj", 7);
    roundtrip(registry, score.to_packet(kprotocol::ProtocolVersion::v1_8), kprotocol::ProtocolVersion::v1_8);

    kprotocol::S3DScoreboardDisplayPacket display{.position = 1, .name = "obj"};
    roundtrip(registry, display.to_packet(kprotocol::ProtocolVersion::v1_8), kprotocol::ProtocolVersion::v1_8);

    std::cout << "ok\n";
}

void test_modern_title_action_bar_scoreboard() {
    std::cout << "  modern title/action bar/scoreboard wire 755/767/770... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    const kprotocol::ProtocolVersion versions[] = {
        kprotocol::ProtocolVersion::v1_17,
        kprotocol::ProtocolVersion::v1_21_1,
        kprotocol::ProtocolVersion::v1_21_5,
    };

    for (const auto version : versions) {
        kprotocol::S62SetTitleTimePacket times{.fade_in = 10, .stay = 70, .fade_out = 20};
        roundtrip(registry, times.to_packet(), version);

        kprotocol::S60SetTitleTextPacket title{.text = "Title"};
        roundtrip(registry, title.to_packet(version), version);

        kprotocol::S61SetTitleSubtitlePacket subtitle{.text = "Sub"};
        roundtrip(registry, subtitle.to_packet(version), version);

        kprotocol::S55ActionBarPacket bar{.text = "Bar"};
        roundtrip(registry, bar.to_packet(version), version);

        kprotocol::S10ClearTitlesPacket clear{.reset_times = false};
        roundtrip(registry, clear.to_packet(), version);

        roundtrip(registry, kprotocol::S3BScoreboardObjectivePacket::make_create("obj", "Demo").to_packet(), version);

        const auto score = kprotocol::S3CScoreboardScorePacket::make_set("Player", "obj", 42);
        roundtrip(registry, score.to_packet(version), version);

        kprotocol::S3DScoreboardDisplayPacket display{.position = 1, .name = "obj"};
        roundtrip(registry, display.to_packet(version), version);
    }

    std::cout << "ok\n";
}

void test_text_component_helpers() {
    std::cout << "  text component helpers... ";
    KPC_CHECK(kprotocol::uses_split_title_packets(kprotocol::ProtocolVersion::v1_17), "split title");
    KPC_CHECK(!kprotocol::uses_split_title_packets(kprotocol::ProtocolVersion::v1_16_5), "legacy title");
    KPC_CHECK(kprotocol::uses_action_bar_packet(kprotocol::ProtocolVersion::v1_17), "action bar");
    KPC_CHECK(kprotocol::uses_nbt_text(kprotocol::ProtocolVersion::v1_21_1), "nbt text");
    KPC_CHECK(!kprotocol::uses_nbt_text(kprotocol::ProtocolVersion::v1_17), "json text");
    KPC_CHECK(!kprotocol::text_component_nbt("Hi").data.empty(), "nbt blob");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "ui_tests:\n";
    try {
        test_text_component_helpers();
        test_legacy_title_action_bar_scoreboard_1_8();
        test_modern_title_action_bar_scoreboard();
        std::cout << "All UI tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "ui_tests failed: " << ex.what() << '\n';
        return 1;
    }
}
