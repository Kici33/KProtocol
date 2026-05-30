#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>

namespace kprotocol {

class C00LoginStartPacket {
public:
    std::string username;
    std::string player_uuid{"00000000-0000-0000-0000-000000000000"};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_start,
            .state = PacketState::login,
            .direction = PacketDirection::serverbound,
            .fields = {
                {"username", username},
                {"signature_present", false},
                {"playerUUID_present", true},
                {"playerUUID", player_uuid}
            }
        };
    }

    static C00LoginStartPacket from_packet(const Packet& packet) {
        std::string uuid_text;
        if (const auto it = packet.fields.find("playerUUID"); it != packet.fields.end()) {
            if (const auto* uuid_string = std::get_if<std::string>(&it->second); uuid_string != nullptr) {
                uuid_text = *uuid_string;
            } else if (const auto* uuid = std::get_if<UUID>(&it->second); uuid != nullptr) {
                uuid_text = uuid->to_string();
            }
        }
        return C00LoginStartPacket{
            .username = require_field<std::string>(packet.fields, "username"),
            .player_uuid = uuid_text.empty()
                ? "00000000-0000-0000-0000-000000000000"
                : uuid_text
        };
    }
};

} // namespace kprotocol
