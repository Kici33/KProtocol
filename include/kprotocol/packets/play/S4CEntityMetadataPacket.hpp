#pragma once

#include "kprotocol/packet.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <vector>

namespace kprotocol {

class S4CEntityMetadataPacket {
public:
    std::int32_t entity_id{};
    std::vector<std::uint8_t> metadata;

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_entity_metadata;
#else
        const std::string_view key = "play.clientbound.entity_metadata";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"entityId", entity_id},
                {"metadata", metadata},
            },
        };
    }

    static S4CEntityMetadataPacket from_packet(const Packet& packet) {
        return S4CEntityMetadataPacket{
            .entity_id = require_field<std::int32_t>(packet.fields, "entityId"),
            .metadata = field_or<std::vector<std::uint8_t>>(packet.fields, "metadata", {}),
        };
    }
};

} // namespace kprotocol
