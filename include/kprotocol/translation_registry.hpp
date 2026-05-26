#pragma once

#include "kprotocol/translation.hpp"
#include "kprotocol/version.hpp"

namespace kprotocol {

/**
 * Central registry for version-specific ID mappings and translation rules.
 * Block state mappings load from data/translation_mappings.bin.gz at runtime.
 * Item, entity, and particle translation are not yet implemented (identity).
 */
class TranslationRegistry {
public:
    static void initialize_all(PacketTranslator& translator);

    static std::int32_t map_block_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    // Identity until item mapping tables are generated.
    static std::int32_t map_item_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    // Identity until entity mapping tables are generated.
    static std::int32_t map_entity_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

    // Identity until particle mapping tables are generated.
    static std::int32_t map_particle_id(
        ProtocolVersion from,
        ProtocolVersion to,
        std::int32_t source_id);

private:
    static bool initialized_;
};

} // namespace kprotocol
