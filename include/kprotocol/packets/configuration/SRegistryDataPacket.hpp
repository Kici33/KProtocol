#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

class SRegistryDataPacket {
public:
    NBTBlob codec;
    std::string id;
    std::vector<std::uint8_t> entries;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::configuration_registry_data,
            .state = PacketState::configuration,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"codec", codec},
                {"id", id},
                {"entries", entries}
            }
        };
    }

    static SRegistryDataPacket from_packet(const Packet& packet) {
        SRegistryDataPacket out;
        if (const auto it = packet.fields.find("codec"); it != packet.fields.end()) {
            if (const auto* codec = std::get_if<NBTBlob>(&it->second); codec != nullptr) {
                out.codec = *codec;
            }
        }
        if (const auto it = packet.fields.find("id"); it != packet.fields.end()) {
            if (const auto* id = std::get_if<std::string>(&it->second); id != nullptr) {
                out.id = *id;
            }
        }
        if (const auto it = packet.fields.find("entries"); it != packet.fields.end()) {
            if (const auto* entries = std::get_if<std::vector<std::uint8_t>>(&it->second); entries != nullptr) {
                out.entries = *entries;
            }
        }
        return out;
    }
};

} // namespace kprotocol
