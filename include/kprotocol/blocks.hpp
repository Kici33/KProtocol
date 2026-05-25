#pragma once

#include "kprotocol/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace kprotocol {

// Legacy 1.8 multi_block_change record: horizontal nibble + y + block state id.
struct MultiBlockChangeRecordLegacy {
    std::uint8_t horizontal_pos{};
    std::uint8_t y{};
    BlockState block{};
};

// Encode/decode the records byte_array field for play.clientbound.multi_block_change (1.8).
[[nodiscard]] std::vector<std::uint8_t> encode_multi_block_records_legacy(
    std::span<const MultiBlockChangeRecordLegacy> records);

[[nodiscard]] std::vector<MultiBlockChangeRecordLegacy> decode_multi_block_records_legacy(
    std::span<const std::uint8_t> bytes);

// Pack chunk section coordinates into the u64 chunkCoordinates bitfield (1.16+).
[[nodiscard]] std::uint64_t pack_chunk_coordinates(
    std::int32_t x,
    std::int32_t y,
    std::int32_t z) noexcept;

[[nodiscard]] Position unpack_chunk_coordinates(std::uint64_t packed) noexcept;

} // namespace kprotocol
