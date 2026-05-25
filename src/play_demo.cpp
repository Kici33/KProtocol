#include "kprotocol/play_demo.hpp"

#include "kprotocol/client_ui.hpp"
#include "kprotocol/entity_metadata.hpp"
#include "kprotocol/packets/login/S02LoginSuccessPacket.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

namespace kprotocol {

namespace {

constexpr ProtocolVersion kConfigurationCutoff = ProtocolVersion::v1_20_2;

} // namespace

bool send_offline_login_success(
    const ClientSession& client,
    const std::string& username,
    const std::string& uuid) {

    if (!client.valid()) {
        return false;
    }

    const auto packet = S02LoginSuccessPacket{.uuid = uuid, .username = username}.to_packet();
    if (!client.send_packet(packet)) {
        return false;
    }

    if (protocol_number(client.protocol_version()) < protocol_number(kConfigurationCutoff)) {
        client.set_state(PacketState::play);
    }
    return true;
}

bool send_play_demo(const ClientSession& client, const ProtocolVersion internal_version) {
    if (!client.valid()) {
        return false;
    }

    if (!send_title(client, {
            .title = "KProtocol",
            .subtitle = "Play demo",
            .fade_in = 10,
            .stay = 60,
            .fade_out = 20,
        })) {
        return false;
    }

    if (!send_action_bar(client, "Action bar from play demo")) {
        return false;
    }

    if (!send_scoreboard_display(client, "demo_obj", 1)) {
        return false;
    }

    if (!send_block_change(client, internal_version, Position{.x = 0, .y = 64, .z = 0}, BlockState{.id = 1})) {
        return false;
    }

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
    const auto key = std::string(generated::packet_keys::play_clientbound_entity_metadata);
#else
    const auto key = std::string("play.clientbound.entity_metadata");
#endif
    Packet metadata_packet{
        .key = key,
        .state = PacketState::play,
        .direction = PacketDirection::clientbound,
        .fields = {
            {"entityId", static_cast<std::int32_t>(1)},
            {"metadata", encode_entity_metadata({})},
        },
    };
    return client.send_packet_direct(metadata_packet, client.protocol_version());
}

} // namespace kprotocol
