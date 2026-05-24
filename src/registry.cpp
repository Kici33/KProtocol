#include "kprotocol/registry.hpp"

#include "kprotocol/types.hpp"

#include <cstring>
#include <functional>
#include <stdexcept>

namespace kprotocol {

namespace {

bool field_present(const PacketFields& fields, const std::string& optional_if) {
    if (optional_if.empty()) {
        return true;
    }
    const auto it = fields.find(optional_if);
    if (it == fields.end()) {
        return false;
    }
    if (const auto* flag = std::get_if<bool>(&it->second); flag != nullptr) {
        return *flag;
    }
    return false;
}

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
    case FieldType::u16_be:
        codec::write_u16(out, std::get<std::uint16_t>(value));
        return;
    case FieldType::byte_array:
        codec::write_bytes(out, std::get<std::vector<std::uint8_t>>(value));
        return;
    case FieldType::i8:
        codec::write_byte(out, std::get<std::int8_t>(value));
        return;
    case FieldType::u8:
        codec::write_ubyte(out, std::get<std::uint8_t>(value));
        return;
    case FieldType::i16_be:
        codec::write_short(out, std::get<std::int16_t>(value));
        return;
    case FieldType::i32_be:
        codec::write_int(out, std::get<std::int32_t>(value));
        return;
    case FieldType::u32_be:
        codec::write_uint(out, std::get<std::uint32_t>(value));
        return;
    case FieldType::i64_be:
        codec::write_long(out, std::get<std::int64_t>(value));
        return;
    case FieldType::u64_be:
        codec::write_ulong(out, std::get<std::uint64_t>(value));
        return;
    case FieldType::f32_be:
        codec::write_float(out, std::get<float>(value));
        return;
    case FieldType::f64_be:
        codec::write_double(out, std::get<double>(value));
        return;
    case FieldType::uuid: {
        const auto& uuid = std::get<UUID>(value);
        out.insert(out.end(), uuid.bytes.begin(), uuid.bytes.end());
        return;
    }
    case FieldType::position: {
        const auto packed = static_cast<std::uint64_t>(std::get<Position>(value).to_long());
        codec::write_ulong(out, packed);
        return;
    }
    case FieldType::rest_buffer: {
        const auto& bytes = std::get<std::vector<std::uint8_t>>(value);
        out.insert(out.end(), bytes.begin(), bytes.end());
        return;
    }
    case FieldType::var_int_array:
        codec::write_var_int_array(out, std::get<std::vector<std::int32_t>>(value));
        return;
    case FieldType::var_long_array:
        codec::write_var_long_array(out, std::get<std::vector<std::int64_t>>(value));
        return;
    case FieldType::slot:
        types::write_slot(out, std::get<types::Slot>(value));
        return;
    case FieldType::optional_nbt:
        types::write_optional_nbt(out, std::get<NBTBlob>(value));
        return;
    }
    throw std::runtime_error("Unsupported field type in write_field");
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
    case FieldType::u16_be:
        return codec::read_u16(input, offset);
    case FieldType::byte_array:
        return codec::read_bytes(input, offset);
    case FieldType::i8:
        return codec::read_byte(input, offset);
    case FieldType::u8:
        return codec::read_ubyte(input, offset);
    case FieldType::i16_be:
        return codec::read_short(input, offset);
    case FieldType::i32_be:
        return codec::read_int(input, offset);
    case FieldType::u32_be:
        return codec::read_uint(input, offset);
    case FieldType::i64_be:
        return codec::read_long(input, offset);
    case FieldType::u64_be:
        return codec::read_ulong(input, offset);
    case FieldType::f32_be:
        return codec::read_float(input, offset);
    case FieldType::f64_be:
        return codec::read_double(input, offset);
    case FieldType::uuid: {
        UUID uuid{};
        if (offset + uuid.bytes.size() > input.size()) {
            throw codec::DecodeError("codec: truncated UUID");
        }
        std::memcpy(uuid.bytes.data(), input.data() + offset, uuid.bytes.size());
        offset += uuid.bytes.size();
        return uuid;
    }
    case FieldType::position: {
        const auto packed = codec::read_ulong(input, offset);
        return Position::from_long(static_cast<std::int64_t>(packed));
    }
    case FieldType::rest_buffer: {
        std::vector<std::uint8_t> bytes(input.begin() + static_cast<std::ptrdiff_t>(offset),
                                        input.end());
        offset = input.size();
        return bytes;
    }
    case FieldType::var_int_array:
        return codec::read_var_int_array(input, offset);
    case FieldType::var_long_array:
        return codec::read_var_long_array(input, offset);
    case FieldType::slot:
        return types::read_slot(input, offset);
    case FieldType::optional_nbt:
        return types::read_optional_nbt(input, offset);
    }
    throw std::runtime_error("Unsupported field type in read_field");
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

const std::vector<FieldSpec>* PacketRegistry::select_field_set(
    const std::map<ProtocolVersion, std::vector<FieldSpec>>& field_sets,
    ProtocolVersion version) noexcept {
    if (field_sets.empty()) {
        return nullptr;
    }
    // upper_bound + step back = the entry with the largest key <= version.
    auto it = field_sets.upper_bound(version);
    if (it == field_sets.begin()) {
        // All keys are > version: no schema covers this version.
        return nullptr;
    }
    --it;
    return &it->second;
}

void PacketRegistry::register_schema(PacketSchema schema) {
    if (schema.key.empty()) {
        throw std::runtime_error("Packet key cannot be empty");
    }

    auto& interner = internal::PacketKeyInterner::instance();
    const auto handle = interner.intern(schema.key);

    // If a schema for this key already exists, merge new ids and field_sets
    // into it rather than overwriting - this lets generators emit incremental
    // registrations across multiple translation units.
    auto existing = schemas_by_handle_.find(handle);
    if (existing == schemas_by_handle_.end()) {
        for (const auto& [version, id] : schema.ids) {
            const IdLookupKey lookup_key{version, schema.state, schema.direction, id};
            if (handle_by_id_.contains(lookup_key)) {
                throw std::runtime_error("Duplicate packet id mapping for key " + schema.key);
            }
            handle_by_id_[lookup_key] = handle;
        }
        schemas_by_handle_.emplace(handle, std::move(schema));
        return;
    }

    PacketSchema& target = existing->second;
    if (target.state != schema.state || target.direction != schema.direction) {
        throw std::runtime_error(
            "Packet key " + schema.key +
            " re-registered with conflicting state/direction");
    }

    for (auto& [version, fields] : schema.field_sets) {
        target.field_sets.insert_or_assign(version, std::move(fields));
    }
    for (const auto& [version, id] : schema.ids) {
        const IdLookupKey lookup_key{version, target.state, target.direction, id};
        if (const auto it = handle_by_id_.find(lookup_key); it != handle_by_id_.end()) {
            if (it->second != handle) {
                throw std::runtime_error(
                    "Duplicate packet id mapping for key " + schema.key);
            }
            continue;
        }
        handle_by_id_[lookup_key] = handle;
        target.ids.insert_or_assign(version, id);
    }
}

void PacketRegistry::register_definition(PacketDefinition definition) {
    PacketSchema schema;
    schema.key = std::move(definition.key);
    schema.state = definition.state;
    schema.direction = definition.direction;
    schema.ids = std::move(definition.ids);
    // Single field set applies to every declared version. Anchor it to the
    // lowest declared version - lookup picks the highest key <= request, so
    // anchoring at the lowest version means every declared version is covered.
    if (!schema.ids.empty()) {
        schema.field_sets.emplace(schema.ids.begin()->first, std::move(definition.fields));
    } else {
        // No ids declared - anchor at v1_8 so any lookup beats it. This keeps
        // legacy tests that register packets without ids working.
        schema.field_sets.emplace(ProtocolVersion::v1_8, std::move(definition.fields));
    }
    register_schema(std::move(schema));
}

const PacketSchema* PacketRegistry::schema_for(std::string_view key) const noexcept {
    if (key.empty()) {
        return nullptr;
    }
    const auto handle = internal::PacketKeyInterner::instance().find(key);
    if (!handle.has_value()) {
        return nullptr;
    }
    const auto it = schemas_by_handle_.find(*handle);
    if (it == schemas_by_handle_.end()) {
        return nullptr;
    }
    return &it->second;
}

const PacketSchema* PacketRegistry::schema_for(
    const ProtocolVersion version,
    const PacketState state,
    const PacketDirection direction,
    const std::int32_t packet_id) const noexcept {
    const IdLookupKey lookup_key{version, state, direction, packet_id};
    const auto id_it = handle_by_id_.find(lookup_key);
    if (id_it == handle_by_id_.end()) {
        return nullptr;
    }
    const auto def_it = schemas_by_handle_.find(id_it->second);
    if (def_it == schemas_by_handle_.end()) {
        return nullptr;
    }
    return &def_it->second;
}

const std::vector<FieldSpec>* PacketRegistry::fields_for(
    std::string_view key, ProtocolVersion version) const noexcept {
    const auto* schema = schema_for(key);
    if (schema == nullptr) {
        return nullptr;
    }
    return select_field_set(schema->field_sets, version);
}

std::optional<PacketDefinition> PacketRegistry::definition_for(std::string_view key) const {
    const auto* schema = schema_for(key);
    if (schema == nullptr) {
        return std::nullopt;
    }
    PacketDefinition out;
    out.key = schema->key;
    out.state = schema->state;
    out.direction = schema->direction;
    out.ids = schema->ids;
    if (!schema->field_sets.empty()) {
        out.fields = schema->field_sets.begin()->second;
    }
    return out;
}

std::optional<PacketDefinition> PacketRegistry::definition_for(
    const ProtocolVersion version,
    const PacketState state,
    const PacketDirection direction,
    const std::int32_t packet_id) const {
    const auto* schema = schema_for(version, state, direction, packet_id);
    if (schema == nullptr) {
        return std::nullopt;
    }
    PacketDefinition out;
    out.key = schema->key;
    out.state = schema->state;
    out.direction = schema->direction;
    out.ids = schema->ids;
    if (const auto* fs = select_field_set(schema->field_sets, version); fs != nullptr) {
        out.fields = *fs;
    } else if (!schema->field_sets.empty()) {
        out.fields = schema->field_sets.begin()->second;
    }
    return out;
}

std::optional<std::int32_t> PacketRegistry::packet_id_for(std::string_view key, const ProtocolVersion version) const {
    const auto* schema = schema_for(key);
    if (schema == nullptr) {
        return std::nullopt;
    }
    const auto id_it = schema->ids.find(version);
    if (id_it != schema->ids.end()) {
        return id_it->second;
    }
    return std::nullopt;
}

std::vector<std::uint8_t> PacketRegistry::encode_packet(
    const Packet& packet,
    const ProtocolVersion version,
    const std::int32_t compression_threshold) const {
    const auto* schema = schema_for(packet.key);
    if (schema == nullptr) {
        throw std::runtime_error("Unknown packet key: " + packet.key);
    }
    if (schema->state != packet.state || schema->direction != packet.direction) {
        throw std::runtime_error("Packet metadata does not match definition");
    }

    const auto packet_id = packet_id_for(packet.key, version);
    if (!packet_id.has_value()) {
        throw std::runtime_error("Packet key has no id for requested version");
    }

    const auto* field_set = select_field_set(schema->field_sets, version);
    if (field_set == nullptr) {
        throw std::runtime_error("Packet key has no field set for requested version");
    }

    std::vector<std::uint8_t> payload;
    for (const auto& field : *field_set) {
        if (!field.optional_if.empty() && !field_present(packet.fields, field.optional_if)) {
            continue;
        }
        const auto it = packet.fields.find(field.name);
        if (it == packet.fields.end()) {
            throw std::runtime_error("Missing required packet field: " + field.name);
        }
        write_field(payload, field, it->second);
    }

    if (compression_threshold < 0) {
        return codec::encode_frame(*packet_id, payload);
    }
    return codec::encode_frame_compressed(*packet_id, payload, compression_threshold);
}

Packet PacketRegistry::decode_packet(
    const codec::EncodedFrame& frame,
    const ProtocolVersion version,
    const PacketState state,
    const PacketDirection direction) const {
    const auto* schema = schema_for(version, state, direction, frame.packet_id);
    if (schema == nullptr) {
        throw std::runtime_error("Unknown packet id for version/state/direction");
    }

    const auto* field_set = select_field_set(schema->field_sets, version);
    if (field_set == nullptr) {
        throw std::runtime_error("Packet has no field set for requested version");
    }

    Packet packet{
        .key = schema->key,
        .state = schema->state,
        .direction = schema->direction,
        .fields = {}
    };

    std::size_t offset = 0;
    const std::span<const std::uint8_t> payload(frame.payload.data(), frame.payload.size());
    for (const auto& field : *field_set) {
        if (!field.optional_if.empty() && !field_present(packet.fields, field.optional_if)) {
            continue;
        }
        packet.fields.emplace(field.name, read_field(payload, offset, field));
    }
    if (offset != payload.size()) {
        throw std::runtime_error("Packet payload has trailing bytes");
    }
    return packet;
}

} // namespace kprotocol
