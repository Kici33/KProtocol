#pragma once

#include "kprotocol/internal/packet_key.hpp"
#include "kprotocol/types.hpp"   // brings in Position, UUID
#include "kprotocol/version.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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
enum class FieldType : std::uint8_t {
    var_int = 0,
    var_long,
    boolean,
    string,
    unsigned_short,
    byte_array,
    i8,
    u8,
    i16_be,
    u16_be,
    i32_be,
    u32_be,
    i64_be,
    u64_be,
    f32_be,
    f64_be,
    uuid,
    position,
    rest_buffer,
    var_int_array,
    var_long_array,
    slot,
    optional_nbt
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
    Position,
    NBTBlob,
    types::Slot,
    std::vector<std::int32_t>,
    std::vector<std::int64_t>
>;

// Dual storage: indexed values for registry encode/decode hot paths; named map
// for handlers and translation lambdas.
struct PacketFields {
    std::unordered_map<std::string, FieldValue> named;
    std::vector<std::optional<FieldValue>> indexed;

    void clear() {
        named.clear();
        indexed.clear();
    }

    void reserve_indexed(std::size_t count) {
        indexed.resize(count);
    }

    void set_indexed(std::size_t index, FieldValue value) {
        if (indexed.size() <= index) {
            indexed.resize(index + 1);
        }
        indexed[index] = std::move(value);
    }

    void emplace(const std::string& name, FieldValue value) {
        named.emplace(name, value);
    }

    [[nodiscard]] auto find(const std::string& name) const noexcept {
        return named.find(name);
    }

    [[nodiscard]] auto find(const std::string& name) noexcept {
        return named.find(name);
    }

    [[nodiscard]] auto end() const noexcept { return named.end(); }
    [[nodiscard]] auto begin() const noexcept { return named.begin(); }
    [[nodiscard]] auto cend() const noexcept { return named.cend(); }
    [[nodiscard]] auto cbegin() const noexcept { return named.cbegin(); }
    [[nodiscard]] bool empty() const noexcept { return named.empty() && indexed.empty(); }
};

struct FieldSpec {
    std::string name;
    FieldType type{};
    std::string optional_if;
};

struct PacketDefinition {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    std::vector<FieldSpec> fields;
    std::map<KnownVersion, std::int32_t> ids;
};

struct PacketSchema {
    PacketKey key;
    PacketState state{};
    PacketDirection direction{};
    std::map<KnownVersion, std::vector<FieldSpec>> field_sets;
    std::map<KnownVersion, std::int32_t> ids;
};

struct Packet {
    PacketKey key;
    internal::PacketKeyHandle key_handle{};
    PacketState state{};
    PacketDirection direction{};
    PacketFields fields;

    void resolve_key_handle() {
        if (key_handle.valid()) {
            return;
        }
        if (key.empty()) {
            return;
        }
        key_handle = internal::PacketKeyInterner::instance().intern(key);
    }

    [[nodiscard]] bool key_matches(std::string_view other) const noexcept {
        if (key_handle.valid()) {
            if (const auto found = internal::PacketKeyInterner::instance().find(other);
                found.has_value()) {
                return key_handle == *found;
            }
        }
        return key == other;
    }
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

[[nodiscard]] inline const FieldValue* field_at(
    const PacketFields& fields,
    const std::vector<FieldSpec>& field_set,
    std::size_t index) noexcept {
    if (index < fields.indexed.size()) {
        if (fields.indexed[index].has_value()) {
            return &*fields.indexed[index];
        }
    }
    if (index >= field_set.size()) {
        return nullptr;
    }
    const auto it = fields.named.find(field_set[index].name);
    if (it == fields.named.end()) {
        return nullptr;
    }
    return &it->second;
}

} // namespace kprotocol
