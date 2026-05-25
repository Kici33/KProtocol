#include "kprotocol/blocks.hpp"

#include "kprotocol/codec.hpp"

namespace kprotocol {

std::vector<std::uint8_t> encode_multi_block_records_legacy(
    const std::span<const MultiBlockChangeRecordLegacy> records) {

    std::vector<std::uint8_t> out;
    codec::write_var_int(out, static_cast<std::int32_t>(records.size()));
    for (const auto& record : records) {
        codec::write_ubyte(out, record.horizontal_pos);
        codec::write_ubyte(out, record.y);
        codec::write_var_int(out, record.block.id);
    }
    return out;
}

std::vector<MultiBlockChangeRecordLegacy> decode_multi_block_records_legacy(
    const std::span<const std::uint8_t> bytes) {

    std::vector<MultiBlockChangeRecordLegacy> records;
    std::size_t offset = 0;
    const auto count = codec::read_var_int(bytes, offset);
    records.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        MultiBlockChangeRecordLegacy record;
        record.horizontal_pos = codec::read_ubyte(bytes, offset);
        record.y = codec::read_ubyte(bytes, offset);
        record.block.id = codec::read_var_int(bytes, offset);
        records.push_back(record);
    }
    return records;
}

std::uint64_t pack_chunk_coordinates(
    const std::int32_t x,
    const std::int32_t y,
    const std::int32_t z) noexcept {

    const auto ux = static_cast<std::uint64_t>(x) & 0x3FFFFFULL;
    const auto uz = static_cast<std::uint64_t>(z) & 0x3FFFFFULL;
    const auto uy = static_cast<std::uint64_t>(y) & 0xFFFFFULL;
    return ux | (uz << 22U) | (uy << 44U);
}

Position unpack_chunk_coordinates(const std::uint64_t packed) noexcept {
    const auto x = static_cast<std::int32_t>(packed << 42U >> 42U);
    const auto z = static_cast<std::int32_t>(packed << 20U >> 42U);
    const auto y = static_cast<std::int32_t>(packed >> 44U);
    return Position{x, y, z};
}

} // namespace kprotocol
