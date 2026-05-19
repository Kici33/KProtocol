#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kprotocol {

class PacketRegistry {
public:
    void register_definition(PacketDefinition definition);

    const PacketDefinition* definition_for(const PacketKey& key) const noexcept;
    const PacketDefinition* definition_for(ProtocolVersion version, PacketState state, PacketDirection direction, std::int32_t packet_id) const noexcept;
    std::optional<std::int32_t> packet_id_for(const PacketKey& key, ProtocolVersion version) const;

    std::vector<std::uint8_t> encode_packet(const Packet& packet, ProtocolVersion version) const;
    Packet decode_packet(const codec::EncodedFrame& frame, ProtocolVersion version, PacketState state, PacketDirection direction) const;

private:
    struct IdLookupKey {
        ProtocolVersion version{};
        PacketState state{};
        PacketDirection direction{};
        std::int32_t packet_id{};

        bool operator==(const IdLookupKey& rhs) const noexcept = default;
    };

    struct IdLookupKeyHash {
        std::size_t operator()(const IdLookupKey& value) const noexcept;
    };

    std::unordered_map<PacketKey, PacketDefinition> definitions_by_key_;
    std::unordered_map<IdLookupKey, PacketKey, IdLookupKeyHash> key_by_id_;
};

} // namespace kprotocol
