#pragma once

#include "kprotocol/translation.hpp"
#include "kprotocol/version.hpp"

#include <unordered_map>

namespace kprotocol {

/**
 * Central registry for version-specific ID mappings and translation rules.
 * Supports block, item, entity, biome, and dimension ID translation.
 */
class TranslationRegistry {
public:
    /**
     * Initialize translation rules for all supported versions.
     * This registers ID mapping rules for blocks, items, entities, etc.
     */
    static void initialize_all(PacketTranslator& translator);

    /**
     * Block ID mapping for a specific version-to-version conversion.
     * Returns the target block ID, or the source ID if no mapping exists.
     */
    static std::int32_t map_block_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    /**
     * Item ID mapping for a specific version-to-version conversion.
     * Returns the target item ID, or the source ID if no mapping exists.
     */
    static std::int32_t map_item_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    /**
     * Entity type ID mapping.
     */
    static std::int32_t map_entity_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    /**
     * Particle ID mapping.
     */
    static std::int32_t map_particle_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

private:
    // Block ID mappings keyed by (from_proto, to_proto) -> {from_id -> to_id}
    static std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> block_mappings_;
    static std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> item_mappings_;
    static std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> entity_mappings_;
    static std::unordered_map<std::string, std::unordered_map<std::int32_t, std::int32_t>> particle_mappings_;

    static bool initialized_;

    /**
     * Helper to create a key for version pair mappings.
     */
    static std::string mapping_key(ProtocolVersion from, ProtocolVersion to);
};

} // namespace kprotocol
