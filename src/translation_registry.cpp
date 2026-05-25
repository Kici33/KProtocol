#include "kprotocol/translation_registry.hpp"

#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

#include <unordered_map>

namespace kprotocol::detail {

const std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>&
embedded_block_mappings();

} // namespace kprotocol::detail

namespace kprotocol {

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::block_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::item_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::entity_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::particle_mappings_;

bool TranslationRegistry::initialized_ = false;

std::string TranslationRegistry::mapping_key(const ProtocolVersion from, const ProtocolVersion to) {
    return name_of(from) + "_to_" + name_of(to);
}

namespace {

PacketFields translate_block_type_field(
    const PacketFields& fields,
    const ProtocolVersion from,
    const ProtocolVersion to) {

    PacketFields out = fields;
    const auto it = out.find("type");
    if (it == out.end()) {
        return out;
    }
    if (const auto* block_id = std::get_if<std::int32_t>(&it->second); block_id != nullptr) {
        it->second = TranslationRegistry::map_block_id(from, to, *block_id);
    }
    return out;
}

void register_block_change_pair(PacketTranslator& translator, ProtocolVersion from, ProtocolVersion to) {
    if (from == to) {
        return;
    }
    translator.register_translation(
        "play.clientbound.block_change",
        from,
        to,
        [from, to](const PacketFields& fields) {
            return translate_block_type_field(fields, from, to);
        });
}

constexpr ProtocolVersion kCatalogVersions[] = {
    ProtocolVersion::v1_8,
    ProtocolVersion::v1_12_2,
    ProtocolVersion::v1_13,
    ProtocolVersion::v1_14,
    ProtocolVersion::v1_16_5,
    ProtocolVersion::v1_17,
    ProtocolVersion::v1_18,
    ProtocolVersion::v1_19,
    ProtocolVersion::v1_20_2,
    ProtocolVersion::v1_20_4,
    ProtocolVersion::v1_21_1,
    ProtocolVersion::v1_21_4,
    ProtocolVersion::v1_21_5,
};

} // namespace

void TranslationRegistry::initialize_all(PacketTranslator& translator) {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    block_mappings_ = detail::embedded_block_mappings();

    for (const auto from : kCatalogVersions) {
        for (const auto to : kCatalogVersions) {
            register_block_change_pair(translator, from, to);
        }
    }

    // Scoreboard display slot field is stable; objective name passes through.
    translator.register_translation(
        "play.clientbound.scoreboard_display_objective",
        ProtocolVersion::v1_21_1,
        ProtocolVersion::v1_8,
        [](const PacketFields& fields) -> PacketFields { return fields; });

    translator.register_translation(
        "play.clientbound.scoreboard_display_objective",
        ProtocolVersion::v1_8,
        ProtocolVersion::v1_21_1,
        [](const PacketFields& fields) -> PacketFields { return fields; });
}

std::int32_t TranslationRegistry::map_block_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {

    if (from == to) {
        return source_id;
    }

    const auto key = mapping_key(from, to);
    const auto it = block_mappings_.find(key);

    if (it == block_mappings_.end()) {
        return source_id;
    }

    const auto& mapping = it->second;
    const auto mapping_it = mapping.find(source_id);

    if (mapping_it == mapping.end()) {
        return source_id;
    }

    return mapping_it->second;
}

std::int32_t TranslationRegistry::map_item_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {

    if (from == to) {
        return source_id;
    }

    const auto key = mapping_key(from, to);
    const auto it = item_mappings_.find(key);

    if (it == item_mappings_.end()) {
        return source_id;
    }

    const auto& mapping = it->second;
    const auto mapping_it = mapping.find(source_id);

    if (mapping_it == mapping.end()) {
        return source_id;
    }

    return mapping_it->second;
}

std::int32_t TranslationRegistry::map_entity_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {

    if (from == to) {
        return source_id;
    }

    const auto key = mapping_key(from, to);
    const auto it = entity_mappings_.find(key);

    if (it == entity_mappings_.end()) {
        return source_id;
    }

    const auto& mapping = it->second;
    const auto mapping_it = mapping.find(source_id);

    if (mapping_it == mapping.end()) {
        return source_id;
    }

    return mapping_it->second;
}

std::int32_t TranslationRegistry::map_particle_id(
    const ProtocolVersion from,
    const ProtocolVersion to,
    const std::int32_t source_id) {

    if (from == to) {
        return source_id;
    }

    const auto key = mapping_key(from, to);
    const auto it = particle_mappings_.find(key);

    if (it == particle_mappings_.end()) {
        return source_id;
    }

    const auto& mapping = it->second;
    const auto mapping_it = mapping.find(source_id);

    if (mapping_it == mapping.end()) {
        return source_id;
    }

    return mapping_it->second;
}

} // namespace kprotocol
