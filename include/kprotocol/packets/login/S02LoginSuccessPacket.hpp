#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>

namespace kprotocol {

class S02LoginSuccessPacket {
public:
    std::string uuid;
    std::string username;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_success,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"uuid", uuid},
                {"username", username},
                {"properties", std::vector<std::uint8_t>{}},
                {"strictErrorHandling", false}
            }
        };
    }

    static S02LoginSuccessPacket from_packet(const Packet& packet) {
        std::string uuid_text;
        if (const auto it = packet.fields.find("uuid"); it != packet.fields.end()) {
            if (const auto* uuid_string = std::get_if<std::string>(&it->second); uuid_string != nullptr) {
                uuid_text = *uuid_string;
            } else if (const auto* uuid = std::get_if<UUID>(&it->second); uuid != nullptr) {
                uuid_text = uuid->to_string();
            }
        }
        if (uuid_text.empty()) {
            throw std::runtime_error("Missing required field: uuid");
        }
        return S02LoginSuccessPacket{
            .uuid = uuid_text,
            .username = require_field<std::string>(packet.fields, "username")
        };
    }
};

} // namespace kprotocol
