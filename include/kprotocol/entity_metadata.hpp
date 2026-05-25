#pragma once

#include "kprotocol/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kprotocol {

// Wire metadata type ids (post-1.9). Values vary slightly by protocol version;
// callers working with decoded blobs should treat unknown types as opaque bytes.
enum class MetadataType : std::int32_t {
    byte = 0,
    var_int = 1,
    var_long = 2,
    float32 = 3,
    string = 4,
    component = 5,
    optional_component = 6,
    item_stack = 7,
    boolean = 8,
    block_pos = 10,
    optional_block_pos = 11,
    direction = 12,
    optional_uuid = 13,
    block_state = 14,
    optional_block_state = 15,
    nbt = 16,
};

struct MetadataEntry {
    std::uint8_t index{};
    MetadataType type{};
    std::vector<std::uint8_t> raw_value;
};

// Parse the metadata blob from play.clientbound.entity_metadata (everything
// after entityId). Returns entries until the 0xFF terminator.
[[nodiscard]] std::vector<MetadataEntry> decode_entity_metadata(std::span<const std::uint8_t> blob);

// Encode metadata entries back to wire bytes (appends 0xFF terminator).
[[nodiscard]] std::vector<std::uint8_t> encode_entity_metadata(const std::vector<MetadataEntry>& entries);

// Well-known metadata indices (common across living entities).
namespace metadata_index {
inline constexpr std::uint8_t custom_name = 2;
inline constexpr std::uint8_t custom_name_visible = 3;
inline constexpr std::uint8_t silent = 4;
} // namespace metadata_index

} // namespace kprotocol
