#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

class S01EncryptionRequestPacket {
public:
    std::string server_id;
    std::vector<std::uint8_t> public_key;
    std::vector<std::uint8_t> verify_token;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_encryption_request,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"serverId", server_id},
                {"publicKey", public_key},
                {"verifyToken", verify_token},
            }
        };
    }

    static S01EncryptionRequestPacket from_packet(const Packet& packet) {
        return S01EncryptionRequestPacket{
            .server_id = require_field<std::string>(packet.fields, "serverId"),
            .public_key = require_field<std::vector<std::uint8_t>>(packet.fields, "publicKey"),
            .verify_token = require_field<std::vector<std::uint8_t>>(packet.fields, "verifyToken")
        };
    }
};

} // namespace kprotocol
