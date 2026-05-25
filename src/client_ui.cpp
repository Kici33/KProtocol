#include "kprotocol/client_ui.hpp"

#include "kprotocol/block_registry.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/packets/play/S23BlockChangePacket.hpp"
#include "kprotocol/packets/play/S3DScoreboardDisplayPacket.hpp"
#include "kprotocol/packets/play/S45TitlePacket.hpp"
#include "kprotocol/packets/play/S55ActionBarPacket.hpp"
#include "kprotocol/packets/play/S60SetTitleTextPacket.hpp"
#include "kprotocol/packets/play/S61SetTitleSubtitlePacket.hpp"
#include "kprotocol/packets/play/S62SetTitleTimePacket.hpp"
#include "kprotocol/text_component.hpp"
#include "kprotocol/translation_registry.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <vector>

namespace kprotocol {

namespace {

bool is_modern_ui_version(const ProtocolVersion version) noexcept {
    return protocol_number(version) >= protocol_number(ProtocolVersion::v1_20_4);
}

std::int32_t title_times_action(const ProtocolVersion version) noexcept {
    return protocol_number(version) <= protocol_number(ProtocolVersion::v1_8)
        ? 2
        : 3;
}

std::vector<std::uint8_t> encode_title_text_tail(const std::string& text) {
    std::vector<std::uint8_t> out;
    codec::write_string(out, text);
    return out;
}

std::vector<std::uint8_t> encode_title_times_tail(
    const std::int32_t fade_in,
    const std::int32_t stay,
    const std::int32_t fade_out) {

    std::vector<std::uint8_t> out;
    codec::write_int(out, fade_in);
    codec::write_int(out, stay);
    codec::write_int(out, fade_out);
    return out;
}

bool send_legacy_title(
    const ClientSession& client,
    const ProtocolVersion client_ver,
    const TitleOptions& options) {

    const auto times_action = title_times_action(client_ver);

    S45TitlePacket times;
    times.action = times_action;
    times.tail = encode_title_times_tail(options.fade_in, options.stay, options.fade_out);
    if (!client.send_packet_direct(times.to_packet(), client_ver)) {
        return false;
    }

    S45TitlePacket title;
    title.action = 0;
    title.tail = encode_title_text_tail(json_text(options.title));
    if (!client.send_packet_direct(title.to_packet(), client_ver)) {
        return false;
    }

    S45TitlePacket subtitle;
    subtitle.action = 1;
    subtitle.tail = encode_title_text_tail(json_text(options.subtitle));
    return client.send_packet_direct(subtitle.to_packet(), client_ver);
}

} // namespace

bool send_title(const ClientSession& client, const TitleOptions& options) {
    if (!client.valid()) {
        return false;
    }
    const auto client_ver = client.protocol_version();

    if (is_modern_ui_version(client_ver)) {
        S62SetTitleTimePacket times{
            .fade_in = options.fade_in,
            .stay = options.stay,
            .fade_out = options.fade_out,
        };
        if (!client.send_packet_direct(times.to_packet(), client_ver)) {
            return false;
        }

        S60SetTitleTextPacket title{.text = options.title};
        if (!client.send_packet_direct(title.to_packet(client_ver), client_ver)) {
            return false;
        }

        S61SetTitleSubtitlePacket subtitle{.text = options.subtitle};
        return client.send_packet_direct(subtitle.to_packet(client_ver), client_ver);
    }

    return send_legacy_title(client, client_ver, options);
}

bool send_action_bar(const ClientSession& client, const std::string& text) {
    if (!client.valid()) {
        return false;
    }
    const auto client_ver = client.protocol_version();

    if (is_modern_ui_version(client_ver)) {
        S55ActionBarPacket packet{.text = text};
        return client.send_packet_direct(packet.to_packet(client_ver), client_ver);
    }

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
    const auto key = std::string(generated::packet_keys::play_clientbound_chat);
#else
    const auto key = "play.clientbound.chat";
#endif
    Packet packet{
        .key = key,
        .state = PacketState::play,
        .direction = PacketDirection::clientbound,
        .fields = {
            {"message", json_text(text)},
            {"position", static_cast<std::int8_t>(2)},
        },
    };
    return client.send_packet_direct(packet, client_ver);
}

bool send_scoreboard_display(
    const ClientSession& client,
    const std::string& objective_name,
    const std::int8_t position) {

    if (!client.valid()) {
        return false;
    }
    const auto client_ver = client.protocol_version();
    S3DScoreboardDisplayPacket packet{
        .position = position,
        .name = objective_name,
    };
    return client.send_packet_direct(packet.to_packet(client_ver), client_ver);
}

bool send_block_change(
    const ClientSession& client,
    const ProtocolVersion internal_version,
    const Position& location,
    const BlockState block) {

    if (!client.valid()) {
        return false;
    }
    const auto client_ver = client.protocol_version();
    S23BlockChangePacket pkt;
    pkt.location = location;
    pkt.block = BlockState{TranslationRegistry::map_block_id(
        internal_version, client_ver, block.id)};

    return client.send_packet_direct(pkt.to_packet(), client_ver);
}

bool send_block_change(
    const ClientSession& client,
    const ProtocolVersion internal_version,
    const Position& location,
    const std::string& block_name) {

    const auto state_id = BlockRegistry::default_state_id(internal_version, block_name);
    if (!state_id.has_value()) {
        return false;
    }
    return send_block_change(client, internal_version, location, BlockState{*state_id});
}

} // namespace kprotocol
