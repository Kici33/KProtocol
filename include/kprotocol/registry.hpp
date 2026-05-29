#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/internal/packet_key.hpp"
#include "kprotocol/packet.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace kprotocol {

class PacketRegistry {
public:
    // Legacy single-version registration. Lifts the PacketDefinition into a
    // PacketSchema whose field_sets apply to every version declared in ids.
    void register_definition(PacketDefinition definition);

    // Multi-version registration. Each entry in field_sets describes the
    // wire layout for a specific protocol version (lookup picks the highest
    // version key <= the requested version).
    void register_schema(PacketSchema schema);

    // Read-only lookup of the full multi-version schema.
    const PacketSchema* schema_for(std::string_view key) const noexcept;
    const PacketSchema* schema_for(ProtocolVersion version, PacketState state, PacketDirection direction, std::int32_t packet_id) const noexcept;
    const PacketSchema* schema_for(KnownVersion version, PacketState state, PacketDirection direction, std::int32_t packet_id) const noexcept {
        return schema_for(to_protocol_version(version), state, direction, packet_id);
    }

    // Returns the field set that applies to `version` for the packet `key`,
    // or nullptr if either the key is unknown or no field set covers that
    // version. The returned pointer is owned by the registry.
    const std::vector<FieldSpec>* fields_for(std::string_view key, ProtocolVersion version) const noexcept;
    const std::vector<FieldSpec>* fields_for(std::string_view key, KnownVersion version) const noexcept {
        return fields_for(key, to_protocol_version(version));
    }

    // String-keyed lookup. std::string and string literals both convert
    // implicitly to std::string_view. Returns a temporary projection of the
    // underlying PacketSchema into the legacy PacketDefinition shape - useful
    // for compatibility, but new code should prefer schema_for().
    [[deprecated("Use schema_for() / fields_for() for per-version lookups.")]]
    std::optional<PacketDefinition> definition_for(std::string_view key) const;
    [[deprecated("Use schema_for() for per-version lookups.")]]
    std::optional<PacketDefinition> definition_for(ProtocolVersion version, PacketState state, PacketDirection direction, std::int32_t packet_id) const;

    std::optional<std::int32_t> packet_id_for(std::string_view key, ProtocolVersion version) const;
    std::optional<std::int32_t> packet_id_for(std::string_view key, KnownVersion version) const {
        return packet_id_for(key, to_protocol_version(version));
    }

    std::vector<std::uint8_t> encode_packet(const Packet& packet, ProtocolVersion version,
                                              std::int32_t compression_threshold = -1) const;
    std::vector<std::uint8_t> encode_packet(const Packet& packet, KnownVersion version,
                                              std::int32_t compression_threshold = -1) const {
        return encode_packet(packet, to_protocol_version(version), compression_threshold);
    }
    Packet decode_packet(const codec::EncodedFrame& frame, ProtocolVersion version, PacketState state, PacketDirection direction) const;
    Packet decode_packet(const codec::EncodedFrame& frame, KnownVersion version, PacketState state, PacketDirection direction) const {
        return decode_packet(frame, to_protocol_version(version), state, direction);
    }

    // Number of registered packet schemas.
    std::size_t size() const noexcept { return schemas_by_handle_.size(); }
    bool empty() const noexcept { return schemas_by_handle_.empty(); }

private:
    struct IdLookupKey {
        KnownVersion version{};
        PacketState state{};
        PacketDirection direction{};
        std::int32_t packet_id{};

        bool operator==(const IdLookupKey& rhs) const noexcept = default;
    };

    struct IdLookupKeyHash {
        std::size_t operator()(const IdLookupKey& value) const noexcept;
    };

    // Looks up the highest version key <= `version` in `field_sets`; returns
    // nullptr if no entry covers it.
    static const std::vector<FieldSpec>* select_field_set(
        const std::map<KnownVersion, std::vector<FieldSpec>>& field_sets,
        KnownVersion version) noexcept;

    // Internal lookups keyed on the interned handle - integer hash + compare,
    // no per-call string hashing on the hot path.
    std::unordered_map<internal::PacketKeyHandle, PacketSchema> schemas_by_handle_;
    std::unordered_map<IdLookupKey, internal::PacketKeyHandle, IdLookupKeyHash> handle_by_id_;
};

} // namespace kprotocol
