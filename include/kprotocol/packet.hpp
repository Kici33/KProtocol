#pragma once

#include "kprotocol/version.hpp"

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
    play
};

enum class PacketDirection : std::uint8_t {
    serverbound,
    clientbound
};

enum class FieldType : std::uint8_t {
    var_int,
    var_long,
    boolean,
    string,
    unsigned_short,
    byte_array
};

using PacketKey = std::string;
using FieldValue = std::variant<std::int32_t, std::int64_t, bool, std::string, std::uint16_t, std::vector<std::uint8_t>>;
using PacketFields = std::unordered_map<std::string, FieldValue>;

struct FieldSpec {
    std::string name;
    FieldType type{};
};

struct PacketDefinition {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    std::vector<FieldSpec> fields;
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
