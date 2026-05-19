#include "kprotocol/registry.hpp"

#include <functional>
#include <stdexcept>

namespace kprotocol {

namespace {

void write_field(std::vector<std::uint8_t>& out, const FieldSpec& spec, const FieldValue& value) {
    switch (spec.type) {
    case FieldType::var_int:
        codec::write_var_int(out, std::get<std::int32_t>(value));
        return;
    case FieldType::var_long:
        codec::write_var_long(out, std::get<std::int64_t>(value));
        return;
    case FieldType::boolean:
        codec::write_bool(out, std::get<bool>(value));
        return;
    case FieldType::string:
        codec::write_string(out, std::get<std::string>(value));
        return;
    case FieldType::unsigned_short:
        codec::write_u16(out, std::get<std::uint16_t>(value));
        return;
    case FieldType::byte_array:
        codec::write_bytes(out, std::get<std::vector<std::uint8_t>>(value));
        return;
    default:
        throw std::runtime_error("Unsupported field type");
    }
}

FieldValue read_field(std::span<const std::uint8_t> input, std::size_t& offset, const FieldSpec& spec) {
    switch (spec.type) {
    case FieldType::var_int:
        return codec::read_var_int(input, offset);
    case FieldType::var_long:
        return codec::read_var_long(input, offset);
    case FieldType::boolean:
        return codec::read_bool(input, offset);
    case FieldType::string:
        return codec::read_string(input, offset);
    case FieldType::unsigned_short:
        return codec::read_u16(input, offset);
    case FieldType::byte_array:
        return codec::read_bytes(input, offset);
    default:
        throw std::runtime_error("Unsupported field type");
    }
}

} // namespace

std::size_t PacketRegistry::IdLookupKeyHash::operator()(const IdLookupKey& value) const noexcept {
    std::size_t seed = 0;
    auto combine = [&seed](const std::size_t h) {
        seed ^= h + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
    };
    combine(std::hash<std::int32_t>{}(protocol_number(value.version)));
    combine(std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(value.state)));
    combine(std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(value.direction)));
    combine(std::hash<std::int32_t>{}(value.packet_id));
    return seed;
}

void PacketRegistry::register_definition(PacketDefinition definition) {
    if (definition.key.empty()) {
        throw std::runtime_error("Packet key cannot be empty");
    }

    for (const auto& [version, id] : definition.ids) {
        const IdLookupKey lookup_key{version, definition.state, definition.direction, id};
        if (key_by_id_.contains(lookup_key)) {
            throw std::runtime_error("Duplicate packet id mapping");
        }
        key_by_id_[lookup_key] = definition.key;
    }

    definitions_by_key_[definition.key] = std::move(definition);
}

const PacketDefinition* PacketRegistry::definition_for(const PacketKey& key) const noexcept {
    const auto it = definitions_by_key_.find(key);
    if (it == definitions_by_key_.end()) {
        return nullptr;
    }
    return &it->second;
}

const PacketDefinition* PacketRegistry::definition_for(
    const ProtocolVersion version,
    const PacketState state,
    const PacketDirection direction,
    const std::int32_t packet_id) const noexcept {
    const IdLookupKey lookup_key{version, state, direction, packet_id};
    const auto id_it = key_by_id_.find(lookup_key);
    if (id_it == key_by_id_.end()) {
        return nullptr;
    }
    return definition_for(id_it->second);
}

std::optional<std::int32_t> PacketRegistry::packet_id_for(const PacketKey& key, const ProtocolVersion version) const {
    const auto* definition = definition_for(key);
    if (definition == nullptr) {
        return std::nullopt;
    }
    const auto id_it = definition->ids.find(version);
    if (id_it != definition->ids.end()) {
        return id_it->second;
    }
    return std::nullopt;
}

std::vector<std::uint8_t> PacketRegistry::encode_packet(const Packet& packet, const ProtocolVersion version) const {
    const auto* definition = definition_for(packet.key);
    if (definition == nullptr) {
        throw std::runtime_error("Unknown packet key: " + packet.key);
    }
    if (definition->state != packet.state || definition->direction != packet.direction) {
        throw std::runtime_error("Packet metadata does not match definition");
    }

    const auto packet_id = packet_id_for(packet.key, version);
    if (!packet_id.has_value()) {
        throw std::runtime_error("Packet key has no id for requested version");
    }

    std::vector<std::uint8_t> payload;
    for (const auto& field : definition->fields) {
        const auto it = packet.fields.find(field.name);
        if (it == packet.fields.end()) {
            throw std::runtime_error("Missing required packet field: " + field.name);
        }
        write_field(payload, field, it->second);
    }

    return codec::encode_frame(*packet_id, payload);
}

Packet PacketRegistry::decode_packet(
    const codec::EncodedFrame& frame,
    const ProtocolVersion version,
    const PacketState state,
    const PacketDirection direction) const {
    const auto* definition = definition_for(version, state, direction, frame.packet_id);
    if (definition == nullptr) {
        throw std::runtime_error("Unknown packet id for version/state/direction");
    }

    Packet packet{
        .key = definition->key,
        .state = definition->state,
        .direction = definition->direction,
        .fields = {}
    };

    std::size_t offset = 0;
    const std::span<const std::uint8_t> payload(frame.payload.data(), frame.payload.size());
    for (const auto& field : definition->fields) {
        packet.fields.emplace(field.name, read_field(payload, offset, field));
    }
    if (offset != payload.size()) {
        throw std::runtime_error("Packet payload has trailing bytes");
    }
    return packet;
}

} // namespace kprotocol
