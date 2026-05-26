#pragma once

#include "kprotocol/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kprotocol {

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

using MetadataValue = std::variant<
    std::monostate,
    std::int8_t,
    std::int32_t,
    std::int64_t,
    float,
    bool,
    std::string,
    Position,
    std::optional<Position>,
    UUID,
    std::optional<UUID>,
    BlockState,
    NBTBlob,
    types::Slot
>;

struct MetadataEntry {
    std::uint8_t index{};
    MetadataType type{};
    MetadataValue value;
    std::vector<std::uint8_t> raw_value;
};

[[nodiscard]] std::vector<MetadataEntry> decode_entity_metadata(std::span<const std::uint8_t> blob);
[[nodiscard]] std::vector<std::uint8_t> encode_entity_metadata(const std::vector<MetadataEntry>& entries);

namespace metadata_index {
inline constexpr std::uint8_t custom_name = 2;
inline constexpr std::uint8_t custom_name_visible = 3;
inline constexpr std::uint8_t silent = 4;
} // namespace metadata_index

} // namespace kprotocol
