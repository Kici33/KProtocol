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
                {"username", username}
            }
        };
    }

    static S02LoginSuccessPacket from_packet(const Packet& packet) {
        return S02LoginSuccessPacket{
            .uuid = require_field<std::string>(packet.fields, "uuid"),
            .username = require_field<std::string>(packet.fields, "username")
        };
    }
};

} // namespace kprotocol
