#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>

namespace kprotocol {

class C15KeepAlivePacket {
public:
    std::int64_t id{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::keep_alive_serverbound,
            .state = PacketState::play,
            .direction = PacketDirection::serverbound,
            .fields = {{"id", id}}
        };
    }

    static C15KeepAlivePacket from_packet(const Packet& packet) {
        return C15KeepAlivePacket{
            .id = require_field<std::int64_t>(packet.fields, "id")
        };
    }
};

} // namespace kprotocol
