#pragma once

#include "kprotocol/translation.hpp"
#include "kprotocol/version.hpp"

#include <string>

namespace kprotocol {

/**
 * Central registry for version-specific ID mappings and translation rules.
 * Block state mappings load from data/translation_mappings.bin.gz at runtime.
 * Installed consumers can use set_block_mappings_path() when the data file
 * lives outside the default installed layout.
 * Item, entity, and particle translation are not yet implemented (identity).
 */
class TranslationRegistry {
public:
    static void initialize_all(PacketTranslator& translator);

    static void set_block_mappings_path(std::string path);
    static void clear_block_mappings_path();
    [[nodiscard]] static bool block_mappings_available() noexcept;

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
};

} // namespace kprotocol
