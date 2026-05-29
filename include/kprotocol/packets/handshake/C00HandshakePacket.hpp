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
                {"protocolVersion", protocol_version},
                {"server_address", server_address},
                {"serverHost", server_address},
                {"server_port", server_port},
                {"serverPort", server_port},
                {"next_state", next_state},
                {"nextState", next_state}
            }
        };
    }

    static C00HandshakePacket from_packet(const Packet& packet) {
        return C00HandshakePacket{
            .protocol_version = field_or<std::int32_t>(
                packet.fields, "protocol_version", field_or<std::int32_t>(packet.fields, "protocolVersion", 0)),
            .server_address = field_or<std::string>(
                packet.fields, "server_address", field_or<std::string>(packet.fields, "serverHost", {})),
            .server_port = field_or<std::uint16_t>(
                packet.fields, "server_port", field_or<std::uint16_t>(packet.fields, "serverPort", 0)),
            .next_state = field_or<std::int32_t>(
                packet.fields, "next_state", field_or<std::int32_t>(packet.fields, "nextState", 0))
        };
    }
};

} // namespace kprotocol
