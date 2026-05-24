#include "kprotocol/translation_registry.hpp"
#include "kprotocol/version.hpp"
#include "kprotocol/packet.hpp"

namespace kprotocol {

// Static member initialization
std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::block_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::item_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::entity_mappings_;

std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>>
    TranslationRegistry::particle_mappings_;

bool TranslationRegistry::initialized_ = false;

std::string TranslationRegistry::mapping_key(ProtocolVersion from, ProtocolVersion to) {
    return std::to_string(static_cast<int>(from)) + "_to_" + std::to_string(static_cast<int>(to));
}

void TranslationRegistry::initialize_all(PacketTranslator& translator) {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    // Entity type IDs sourced from minecraft-data (name-stable bridge).
    entity_mappings_[mapping_key(ProtocolVersion::v1_20_4, ProtocolVersion::v1_16_5)][120] = 102; // zombie
    entity_mappings_[mapping_key(ProtocolVersion::v1_16_5, ProtocolVersion::v1_20_4)][102] = 120;
    entity_mappings_[mapping_key(ProtocolVersion::v1_20_4, ProtocolVersion::v1_16_5)][1] = 1;
    entity_mappings_[mapping_key(ProtocolVersion::v1_16_5, ProtocolVersion::v1_20_4)][1] = 1;

    // Block state IDs (default states) that shifted between these versions.
    block_mappings_[mapping_key(ProtocolVersion::v1_20_4, ProtocolVersion::v1_16_5)][21] = 20; // dark_oak_planks
    block_mappings_[mapping_key(ProtocolVersion::v1_16_5, ProtocolVersion::v1_20_4)][20] = 21;

    translator.register_translation(
        "play.clientbound.block_change",
        ProtocolVersion::v1_20_4,
        ProtocolVersion::v1_16_5,
        [](const PacketFields& fields) -> PacketFields {
            PacketFields out = fields;
            const auto it = out.find("type");
            if (it == out.end()) {
                return out;
            }
            if (const auto* block_id = std::get_if<std::int32_t>(&it->second); block_id != nullptr) {
                it->second = map_block_id(
                    ProtocolVersion::v1_20_4,
                    ProtocolVersion::v1_16_5,
                    *block_id);
            }
            return out;
        });

    translator.register_translation(
        "play.clientbound.block_change",
        ProtocolVersion::v1_16_5,
        ProtocolVersion::v1_20_4,
        [](const PacketFields& fields) -> PacketFields {
            PacketFields out = fields;
            const auto it = out.find("type");
            if (it == out.end()) {
                return out;
            }
            if (const auto* block_id = std::get_if<std::int32_t>(&it->second); block_id != nullptr) {
                it->second = map_block_id(
                    ProtocolVersion::v1_16_5,
                    ProtocolVersion::v1_20_4,
                    *block_id);
            }
            return out;
        });
}

std::int32_t TranslationRegistry::map_block_id(
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {

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
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {

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
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {

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
    ProtocolVersion from,
    ProtocolVersion to,
    std::int32_t source_id) {

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
