#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>

namespace kprotocol {

class S03SetCompressionPacket {
public:
    std::int32_t threshold{-1};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_set_compression,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {{"threshold", threshold}}
        };
    }

    static S03SetCompressionPacket from_packet(const Packet& packet) {
        return S03SetCompressionPacket{
            .threshold = require_field<std::int32_t>(packet.fields, "threshold")
        };
    }
};

} // namespace kprotocol
