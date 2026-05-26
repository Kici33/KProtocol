#include "kprotocol/play_demo.hpp"

#include "kprotocol/block_registry.hpp"
#include "kprotocol/blocks.hpp"
#include "kprotocol/client_ui.hpp"
#include "kprotocol/entity_metadata.hpp"
#include "kprotocol/metadata_registry.hpp"
#include "kprotocol/packets/login/S02LoginSuccessPacket.hpp"
#include "kprotocol/packets/play/S22MultiBlockChangePacket.hpp"
#include "kprotocol/packets/play/S24BlockActionPacket.hpp"
#include "kprotocol/packets/play/STileEntityDataPacket.hpp"

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

    static const ScoreboardLine kDemoLines[] = {
        {.entry = "Player", .value = 10},
        {.entry = "KProtocol", .value = 100},
    };
    if (!send_scoreboard_sidebar(client, "demo_obj", "Demo", kDemoLines, 1)) {
        return false;
    }

    if (!send_block_change(
            client,
            internal_version,
            Position{.x = 0, .y = 64, .z = 0},
            BlockRegistry::default_state(internal_version, "stone"))) {
        return false;
    }

    const auto client_ver = client.protocol_version();
    S22MultiBlockChangePacket multi;
    multi.wire_version = client_ver;
    if (protocol_number(client_ver) <= protocol_number(ProtocolVersion::v1_12_2)) {
        multi.chunk_x = 0;
        multi.chunk_z = 0;
        multi.legacy_records.push_back(MultiBlockChangeRecordLegacy{
            .horizontal_pos = 5,
            .y = 64,
            .block = BlockRegistry::translate(
                BlockRegistry::default_state(internal_version, "stone"),
                internal_version,
                client_ver),
        });
    } else {
        multi.chunk_coordinates = pack_chunk_coordinates(0, 64, 0);
        multi.var_int_records = {BlockRegistry::translate(
            BlockRegistry::default_state(internal_version, "stone"),
            internal_version,
            client_ver).id};
    }
    if (!client.send_packet_direct(multi.to_packet(), client_ver)) {
        return false;
    }

    STileEntityDataPacket tile;
    tile.wire_version = client_ver;
    tile.location = Position{.x = 0, .y = 64, .z = 0};
    tile.action = 1;
    if (!client.send_packet_direct(tile.to_packet(), client_ver)) {
        return false;
    }

    S24BlockActionPacket block_action;
    block_action.location = Position{.x = 0, .y = 64, .z = 0};
    block_action.block = BlockRegistry::translate(
        BlockRegistry::default_state(internal_version, "stone"),
        internal_version,
        client_ver);
    if (!client.send_packet_direct(block_action.to_packet(), client_ver)) {
        return false;
    }

    std::vector<MetadataEntry> metadata_entries;
    if (const auto silent_index = MetadataRegistry::index_for("zombie", "silent")) {
        MetadataEntry entry;
        entry.index = *silent_index;
        entry.type = MetadataType::boolean;
        entry.raw_value = {0x00};
        metadata_entries.push_back(entry);
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
            {"metadata", encode_entity_metadata(metadata_entries)},
        },
    };
    return client.send_packet_direct(metadata_packet, client.protocol_version());
}

} // namespace kprotocol
