#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>
#include <vector>

namespace kprotocol {

class C01EncryptionResponsePacket {
public:
    std::vector<std::uint8_t> shared_secret;
    std::vector<std::uint8_t> verify_token;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_encryption_response,
            .state = PacketState::login,
            .direction = PacketDirection::serverbound,
            .fields = {
                {"sharedSecret", shared_secret},
                {"verifyToken", verify_token},
            }
        };
    }

    static C01EncryptionResponsePacket from_packet(const Packet& packet) {
        return C01EncryptionResponsePacket{
            .shared_secret = require_field<std::vector<std::uint8_t>>(packet.fields, "sharedSecret"),
            .verify_token = require_field<std::vector<std::uint8_t>>(packet.fields, "verifyToken")
        };
    }
};

} // namespace kprotocol
