#pragma once

#include "kprotocol/types.hpp"   // brings in Position, UUID
#include "kprotocol/version.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace kprotocol {

enum class PacketState : std::uint8_t {
    handshaking,
    status,
    login,
    play,
    configuration  // Added in 1.20.2 (wire 764).
};

enum class PacketDirection : std::uint8_t {
    serverbound,
    clientbound
};

// Wire-level field types.
//
// Naming: types with explicit endianness suffix (_be) are encoded big-endian
// per the Mojang protocol convention. var_int/var_long are the unsigned LEB128
// flavor with 5/10-byte caps. position is the 8-byte XYZ packing used since
// 1.14 (pre-1.14 used a different packing - generators should emit
// position_pre114 when that variant is added in a future wave).
enum class FieldType : std::uint8_t {
    var_int = 0,
    var_long,
    boolean,
    string,
    unsigned_short,
    byte_array,

    // Fixed-width integers (big-endian on the wire).
    i8,
    u8,
    i16_be,
    u16_be,        // synonym of unsigned_short for generator output
    i32_be,
    u32_be,
    i64_be,
    u64_be,

    // IEEE-754 (big-endian on the wire).
    f32_be,
    f64_be,

    // Aggregates.
    uuid,          // 16 raw bytes, network order
    position,      // 8 bytes; (x:26, y:12, z:26) packing (post-1.14)

    // Trailing-bytes payload. UNLIKE byte_array, this is NOT length-prefixed -
    // it consumes the remaining bytes of the packet body. Used by the
    // generator as a fallback when a packet contains compound fields that
    // the catalog cannot represent yet, so the packet can still be encoded
    // and decoded round-trip-safely as a blob. Must be the last field in a
    // schema (the decoder takes everything that's left).
    rest_buffer
};

using PacketKey = std::string;

using FieldValue = std::variant<
    std::int32_t,
    std::int64_t,
    bool,
    std::string,
    std::uint16_t,
    std::vector<std::uint8_t>,
    std::int8_t,
    std::uint8_t,
    std::int16_t,
    std::uint32_t,
    std::uint64_t,
    float,
    double,
    UUID,
    Position
>;

using PacketFields = std::unordered_map<std::string, FieldValue>;

struct FieldSpec {
    std::string name;
    FieldType type{};
};

// Legacy single-version definition. Equivalent to a PacketSchema with one
// field set that applies to every declared version. Kept for source-level
// compatibility - existing code (baseline_packets.cpp, examples) constructs
// these directly. The registry transparently lifts them into PacketSchema.
struct PacketDefinition {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    std::vector<FieldSpec> fields;
    std::map<ProtocolVersion, std::int32_t> ids;
};

// Multi-version packet schema.
//
// A single PacketSchema describes one logical packet (e.g. "play.chat_message"
// or "login.success") across every protocol version it appears in. The wire
// layout often differs between versions; field_sets stores one entry per
// version, and lookup picks the entry whose ProtocolVersion is the highest
// known key <= the requested version. If no field set is found for the
// requested version, the schema is considered absent for that version (an
// error is raised when encoding, and the packet is rejected when decoding).
//
// IDs follow the same lookup rule via PacketRegistry::packet_id_for; the
// ids map is intentionally per-version because IDs change frequently.
struct PacketSchema {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    std::map<ProtocolVersion, std::vector<FieldSpec>> field_sets;
    std::map<ProtocolVersion, std::int32_t> ids;
};

struct Packet {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    PacketFields fields;
};

template <typename T>
inline const T& require_field(const PacketFields& fields, const std::string& name) {
    const auto it = fields.find(name);
    if (it == fields.end()) {
        throw std::runtime_error("Missing required field: " + name);
    }
    const auto* value = std::get_if<T>(&it->second);
    if (value == nullptr) {
        throw std::runtime_error("Invalid field type for: " + name);
    }
    return *value;
}

template <typename T>
inline T field_or(const PacketFields& fields, const std::string& name, const T& fallback) {
    const auto it = fields.find(name);
    if (it == fields.end()) {
        return fallback;
    }
    if (const auto* value = std::get_if<T>(&it->second); value != nullptr) {
        return *value;
    }
    return fallback;
}

} // namespace kprotocol
