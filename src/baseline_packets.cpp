#include "kprotocol/baseline_packets.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace kprotocol {

namespace {

std::map<KnownVersion, std::int32_t> ids(const std::int32_t v1_8, const std::int32_t v1_12_2, const std::int32_t v1_16_5, const std::int32_t v1_20_4, const std::int32_t v1_21_1) {
    return {
        {KnownVersion::v1_8, v1_8},
        {KnownVersion::v1_12_2, v1_12_2},
        {KnownVersion::v1_16_5, v1_16_5},
        {KnownVersion::v1_20_4, v1_20_4},
        {KnownVersion::v1_21_1, v1_21_1}
    };
}

std::string strip_uuid_dashes(std::string value) {
    value.erase(std::remove(value.begin(), value.end(), '-'), value.end());
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string add_uuid_dashes(const std::string& raw) {
    if (raw.size() != 32U) {
        return raw;
    }
    std::string output;
    output.reserve(36U);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (i == 8U || i == 12U || i == 16U || i == 20U) {
            output.push_back('-');
        }
        output.push_back(raw[i]);
    }
    return output;
}

void register_definition_if_missing(PacketRegistry& registry, PacketDefinition definition) {
    if (registry.schema_for(definition.key) != nullptr) {
        return;
    }
    registry.register_definition(std::move(definition));
}

} // namespace

void register_baseline_packets(PacketRegistry& registry, PacketTranslator& translator) {
    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::handshake,
        .state = PacketState::handshaking,
        .direction = PacketDirection::serverbound,
        .fields = {
            {"protocol_version", FieldType::var_int},
            {"server_address", FieldType::string},
            {"server_port", FieldType::unsigned_short},
            {"next_state", FieldType::var_int}
        },
        .ids = ids(0x00, 0x00, 0x00, 0x00, 0x00)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::status_request,
        .state = PacketState::status,
        .direction = PacketDirection::serverbound,
        .fields = {},
        .ids = ids(0x00, 0x00, 0x00, 0x00, 0x00)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::status_response,
        .state = PacketState::status,
        .direction = PacketDirection::clientbound,
        .fields = {{"json_response", FieldType::string}},
        .ids = ids(0x00, 0x00, 0x00, 0x00, 0x00)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::ping_request,
        .state = PacketState::status,
        .direction = PacketDirection::serverbound,
        .fields = {{"payload", FieldType::var_long}},
        .ids = ids(0x01, 0x01, 0x01, 0x01, 0x01)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::pong_response,
        .state = PacketState::status,
        .direction = PacketDirection::clientbound,
        .fields = {{"payload", FieldType::var_long}},
        .ids = ids(0x01, 0x01, 0x01, 0x01, 0x01)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_start,
        .state = PacketState::login,
        .direction = PacketDirection::serverbound,
        .fields = {{"username", FieldType::string}},
        .ids = ids(0x00, 0x00, 0x00, 0x00, 0x00)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_success,
        .state = PacketState::login,
        .direction = PacketDirection::clientbound,
        .fields = {
            {"uuid", FieldType::string},
            {"username", FieldType::string}
        },
        .ids = ids(0x02, 0x02, 0x02, 0x02, 0x02)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_disconnect,
        .state = PacketState::login,
        .direction = PacketDirection::clientbound,
        .fields = {{"reason_json", FieldType::string}},
        .ids = ids(0x00, 0x00, 0x00, 0x00, 0x00)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_encryption_request,
        .state = PacketState::login,
        .direction = PacketDirection::clientbound,
        .fields = {
            {"serverId", FieldType::string},
            {"publicKey", FieldType::byte_array},
            {"verifyToken", FieldType::byte_array}
        },
        .ids = ids(0x01, 0x01, 0x01, 0x01, 0x01)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_encryption_response,
        .state = PacketState::login,
        .direction = PacketDirection::serverbound,
        .fields = {
            {"sharedSecret", FieldType::byte_array},
            {"verifyToken", FieldType::byte_array}
        },
        .ids = ids(0x01, 0x01, 0x01, 0x01, 0x01)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::login_set_compression,
        .state = PacketState::login,
        .direction = PacketDirection::clientbound,
        .fields = {{"threshold", FieldType::var_int}},
        .ids = ids(0x03, 0x03, 0x03, 0x03, 0x03)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::keep_alive_serverbound,
        .state = PacketState::play,
        .direction = PacketDirection::serverbound,
        .fields = {{"id", FieldType::var_long}},
        .ids = ids(0x00, 0x0B, 0x10, 0x15, 0x15)
    });

    register_definition_if_missing(registry, PacketDefinition{
        .key = packet_keys::keep_alive_clientbound,
        .state = PacketState::play,
        .direction = PacketDirection::clientbound,
        .fields = {{"id", FieldType::var_long}},
        .ids = ids(0x00, 0x1F, 0x21, 0x24, 0x24)
    });

    translator.register_translation(packet_keys::login_success, ProtocolVersion::v1_21_1, ProtocolVersion::v1_8, [](const PacketFields& fields) {
        PacketFields out = fields;
        const auto it = out.find("uuid");
        if (it != out.end()) {
            if (const auto* uuid = std::get_if<std::string>(&it->second); uuid != nullptr) {
                it->second = strip_uuid_dashes(*uuid);
            }
        }
        return out;
    });

    translator.register_translation(packet_keys::login_success, ProtocolVersion::v1_8, ProtocolVersion::v1_21_1, [](const PacketFields& fields) {
        PacketFields out = fields;
        const auto it = out.find("uuid");
        if (it != out.end()) {
            if (const auto* uuid = std::get_if<std::string>(&it->second); uuid != nullptr) {
                it->second = add_uuid_dashes(*uuid);
            }
        }
        return out;
    });
}

} // namespace kprotocol
