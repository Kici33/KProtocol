#include "kprotocol/client_ui.hpp"

#include "kprotocol/codec.hpp"
#include "kprotocol/packets/play/S23BlockChangePacket.hpp"
#include "kprotocol/packets/play/S45TitlePacket.hpp"
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

std::string json_text(const std::string& plain) {
    std::string out = R"({"text":")";
    for (const char c : plain) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out += R"("})";
    return out;
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
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const auto time_key = std::string(generated::packet_keys::play_clientbound_set_title_time);
        const auto text_key = std::string(generated::packet_keys::play_clientbound_set_title_text);
        const auto sub_key = std::string(generated::packet_keys::play_clientbound_set_title_subtitle);
#else
        const auto time_key = "play.clientbound.set_title_time";
        const auto text_key = "play.clientbound.set_title_text";
        const auto sub_key = "play.clientbound.set_title_subtitle";
#endif
        Packet times{
            .key = time_key,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"fadeIn", options.fade_in},
                {"stay", options.stay},
                {"fadeOut", options.fade_out},
            },
        };
        if (!client.send_packet_direct(times, client_ver)) {
            return false;
        }
        NBTBlob absent{};
        Packet set_title{
            .key = text_key,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"text", absent}},
        };
        if (!client.send_packet_direct(set_title, client_ver)) {
            return false;
        }
        Packet set_sub{
            .key = sub_key,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"text", absent}},
        };
        return client.send_packet_direct(set_sub, client_ver);
    }

    return send_legacy_title(client, client_ver, options);
}

bool send_action_bar(const ClientSession& client, const std::string& text) {
    if (!client.valid()) {
        return false;
    }
    const auto client_ver = client.protocol_version();

    if (is_modern_ui_version(client_ver)) {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const auto key = std::string(generated::packet_keys::play_clientbound_action_bar);
#else
        const auto key = "play.clientbound.action_bar";
#endif
        NBTBlob absent{};
        Packet packet{
            .key = key,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"text", absent}},
        };
        return client.send_packet_direct(packet, client_ver);
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
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
    const auto key = std::string(generated::packet_keys::play_clientbound_scoreboard_display_objective);
#else
    const auto key = "play.clientbound.scoreboard_display_objective";
#endif
    Packet packet{
        .key = key,
        .state = PacketState::play,
        .direction = PacketDirection::clientbound,
        .fields = {
            {"position", position},
            {"name", objective_name},
        },
    };
    return client.send_packet_direct(packet, client.protocol_version());
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

} // namespace kprotocol
