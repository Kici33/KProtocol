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

    // Returns the field set that applies to `version` for the packet `key`,
    // or nullptr if either the key is unknown or no field set covers that
    // version. The returned pointer is owned by the registry.
    const std::vector<FieldSpec>* fields_for(std::string_view key, ProtocolVersion version) const noexcept;

    // String-keyed lookup. std::string and string literals both convert
    // implicitly to std::string_view. Returns a temporary projection of the
    // underlying PacketSchema into the legacy PacketDefinition shape - useful
    // for compatibility, but new code should prefer schema_for().
    [[deprecated("Use schema_for() / fields_for() for per-version lookups.")]]
    std::optional<PacketDefinition> definition_for(std::string_view key) const;
    [[deprecated("Use schema_for() for per-version lookups.")]]
    std::optional<PacketDefinition> definition_for(ProtocolVersion version, PacketState state, PacketDirection direction, std::int32_t packet_id) const;

    std::optional<std::int32_t> packet_id_for(std::string_view key, ProtocolVersion version) const;

    std::vector<std::uint8_t> encode_packet(const Packet& packet, ProtocolVersion version) const;
    Packet decode_packet(const codec::EncodedFrame& frame, ProtocolVersion version, PacketState state, PacketDirection direction) const;

    // Number of registered packet schemas.
    std::size_t size() const noexcept { return schemas_by_handle_.size(); }
    bool empty() const noexcept { return schemas_by_handle_.empty(); }

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

    // Looks up the highest version key <= `version` in `field_sets`; returns
    // nullptr if no entry covers it.
    static const std::vector<FieldSpec>* select_field_set(
        const std::map<ProtocolVersion, std::vector<FieldSpec>>& field_sets,
        ProtocolVersion version) noexcept;

    // Internal lookups keyed on the interned handle - integer hash + compare,
    // no per-call string hashing on the hot path.
    std::unordered_map<internal::PacketKeyHandle, PacketSchema> schemas_by_handle_;
    std::unordered_map<IdLookupKey, internal::PacketKeyHandle, IdLookupKeyHash> handle_by_id_;
};

} // namespace kprotocol
