#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <string>

namespace kprotocol {

class STileEntityDataPacket {
public:
    ProtocolVersion wire_version{ProtocolVersion::v1_21_1};
    Position location{};
    std::int32_t action{};
    NBTBlob nbt{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_tile_entity_data;
#else
        const std::string_view key = "play.clientbound.tile_entity_data";
#endif
        Packet packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"location", location},
                {"nbtData", nbt},
            },
        };
        if (protocol_number(wire_version) >= protocol_number(ProtocolVersion::v1_18_2)) {
            packet.fields["action"] = action;
        } else {
            packet.fields["action"] = static_cast<std::uint8_t>(action);
        }
        return packet;
    }

    static STileEntityDataPacket from_packet(const Packet& packet, ProtocolVersion version) {
        STileEntityDataPacket out{
            .wire_version = version,
            .location = require_field<Position>(packet.fields, "location"),
            .nbt = field_or<NBTBlob>(packet.fields, "nbtData", {}),
        };
        if (protocol_number(version) >= protocol_number(ProtocolVersion::v1_18_2)) {
            out.action = require_field<std::int32_t>(packet.fields, "action");
        } else {
            out.action = require_field<std::uint8_t>(packet.fields, "action");
        }
        return out;
    }
};

} // namespace kprotocol
