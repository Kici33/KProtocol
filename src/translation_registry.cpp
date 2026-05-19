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

/**
 * Initialize translation rules for all supported versions.
 * This registers handlers for packets that contain version-specific IDs.
 */
void TranslationRegistry::initialize_all(PacketTranslator& translator) {
    if (initialized_) {
        return;
    }
    initialized_ = true;
    
    (void)translator; // Suppress unused parameter warning
    
    // Register packet translation rules
    // These rules handle packets that contain block IDs, item IDs, etc. that need translation
    
    // Example: Set Block packet (S35 or similar) - translates the block ID
    // translator.register_translation(
    //     "SetBlock",
    //     ProtocolVersion::V1_20_4,
    //     ProtocolVersion::V1_16_5,
    //     [](const PacketFields& fields) -> PacketFields {
    //         PacketFields translated = fields;
    //         if (const auto* block_id = std::get_if<std::int32_t>(&fields.at("block_id"))) {
    //             translated["block_id"] = map_block_id(
    //                 ProtocolVersion::V1_20_4,
    //                 ProtocolVersion::V1_16_5,
    //                 *block_id
    //             );
    //         }
    //         return translated;
    //     }
    // );
    
    // More translation rules can be registered here for:
    // - Chunk Data (contains block palette)
    // - Spawn Entity (contains entity type ID)
    // - Map Item Stack (contains item ID)
    // - Sound Effect (contains sound ID)
    // - Particle (contains particle type ID)
}

/**
 * Block ID mapping helper - translates a block ID from one version to another.
 * Uses the extracted block name mappings as the bridge.
 */
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
        // No mapping found, return source ID unchanged
        return source_id;
    }
    
    const auto& mapping = it->second;
    const auto mapping_it = mapping.find(source_id);
    
    if (mapping_it == mapping.end()) {
        // Block not found in mapping, return source ID
        return source_id;
    }
    
    return mapping_it->second;
}

/**
 * Item ID mapping helper.
 */
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

/**
 * Entity type ID mapping helper.
 */
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

/**
 * Particle ID mapping helper.
 */
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
