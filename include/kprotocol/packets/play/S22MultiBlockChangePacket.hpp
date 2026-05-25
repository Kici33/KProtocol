#pragma once

#include "kprotocol/blocks.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <string>
#include <vector>

namespace kprotocol {

class S22MultiBlockChangePacket {
public:
    ProtocolVersion wire_version{ProtocolVersion::v1_8};
    std::int32_t chunk_x{};
    std::int32_t chunk_z{};
    std::uint64_t chunk_coordinates{};
    bool not_trust_edges{};
    std::vector<MultiBlockChangeRecordLegacy> legacy_records;
    std::vector<std::int64_t> packed_records;
    std::vector<std::int32_t> var_int_records;

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_multi_block_change;
#else
        const std::string_view key = "play.clientbound.multi_block_change";
#endif
        Packet packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
        };
        if (protocol_number(wire_version) <= protocol_number(ProtocolVersion::v1_12_2)) {
            packet.fields["chunkX"] = chunk_x;
            packet.fields["chunkZ"] = chunk_z;
            packet.fields["records"] = encode_multi_block_records_legacy(legacy_records);
            return packet;
        }
        packet.fields["chunkCoordinates"] = chunk_coordinates;
        if (protocol_number(wire_version) >= protocol_number(ProtocolVersion::v1_16_5) &&
            protocol_number(wire_version) < protocol_number(ProtocolVersion::v1_20_2)) {
            packet.fields["notTrustEdges"] = not_trust_edges;
            packet.fields["records"] = packed_records;
            return packet;
        }
        packet.fields["records"] = var_int_records;
        return packet;
    }

    static S22MultiBlockChangePacket from_packet(const Packet& packet, ProtocolVersion version) {
        S22MultiBlockChangePacket out{.wire_version = version};
        if (protocol_number(version) <= protocol_number(ProtocolVersion::v1_12_2)) {
            out.chunk_x = require_field<std::int32_t>(packet.fields, "chunkX");
            out.chunk_z = require_field<std::int32_t>(packet.fields, "chunkZ");
            const auto& bytes = require_field<std::vector<std::uint8_t>>(packet.fields, "records");
            out.legacy_records = decode_multi_block_records_legacy(bytes);
            return out;
        }
        out.chunk_coordinates = require_field<std::uint64_t>(packet.fields, "chunkCoordinates");
        if (protocol_number(version) >= protocol_number(ProtocolVersion::v1_16_5) &&
            protocol_number(version) < protocol_number(ProtocolVersion::v1_20_2)) {
            out.not_trust_edges = field_or<bool>(packet.fields, "notTrustEdges", false);
            out.packed_records = require_field<std::vector<std::int64_t>>(packet.fields, "records");
            return out;
        }
        out.var_int_records = require_field<std::vector<std::int32_t>>(packet.fields, "records");
        return out;
    }
};

} // namespace kprotocol
