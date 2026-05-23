#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>
#include <string>

namespace kprotocol {

class C00HandshakePacket {
public:
    std::int32_t protocol_version{};
    std::string server_address;
    std::uint16_t server_port{};
    std::int32_t next_state{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::handshake,
            .state = PacketState::handshaking,
            .direction = PacketDirection::serverbound,
            .fields = {
                {"protocol_version", protocol_version},
                {"server_address", server_address},
                {"server_port", server_port},
                {"next_state", next_state}
            }
        };
    }

    static C00HandshakePacket from_packet(const Packet& packet) {
        return C00HandshakePacket{
            .protocol_version = require_field<std::int32_t>(packet.fields, "protocol_version"),
            .server_address = require_field<std::string>(packet.fields, "server_address"),
            .server_port = require_field<std::uint16_t>(packet.fields, "server_port"),
            .next_state = require_field<std::int32_t>(packet.fields, "next_state")
        };
    }
};

} // namespace kprotocol
