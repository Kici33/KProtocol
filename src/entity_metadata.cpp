#include "kprotocol/entity_metadata.hpp"

#include "kprotocol/codec.hpp"

#include <stdexcept>

namespace kprotocol {

namespace {

void append_var_int(std::vector<std::uint8_t>& out, std::int32_t value) {
    codec::write_var_int(out, value);
}

std::int32_t read_var_int(std::span<const std::uint8_t> input, std::size_t& offset) {
    return codec::read_var_int(input, offset);
}

std::vector<std::uint8_t> read_rest(std::span<const std::uint8_t> input, std::size_t& offset) {
    if (offset >= input.size()) {
        return {};
    }
    return {input.begin() + static_cast<std::ptrdiff_t>(offset), input.end()};
}

} // namespace

std::vector<MetadataEntry> decode_entity_metadata(const std::span<const std::uint8_t> blob) {
    std::vector<MetadataEntry> entries;
    std::size_t offset = 0;
    while (offset < blob.size()) {
        const auto index = codec::read_ubyte(blob, offset);
        if (index == 0xFFU) {
            break;
        }
        const auto type_id = read_var_int(blob, offset);
        MetadataEntry entry;
        entry.index = index;
        entry.type = static_cast<MetadataType>(type_id);
        const std::size_t value_start = offset;
        switch (entry.type) {
        case MetadataType::byte:
            (void)codec::read_byte(blob, offset);
            break;
        case MetadataType::var_int:
            (void)read_var_int(blob, offset);
            break;
        case MetadataType::var_long:
            (void)codec::read_var_long(blob, offset);
            break;
        case MetadataType::float32:
            (void)codec::read_float(blob, offset);
            break;
        case MetadataType::string:
            (void)codec::read_string(blob, offset);
            break;
        case MetadataType::boolean:
            (void)codec::read_bool(blob, offset);
            break;
        case MetadataType::block_pos: {
            (void)codec::read_ulong(blob, offset);
            break;
        }
        default:
            // Unknown or complex type: consume until next entry is ambiguous without
            // full schema; store remainder as opaque from this point for round-trip.
            entry.raw_value = read_rest(blob, value_start);
            offset = blob.size();
            entries.push_back(std::move(entry));
            return entries;
        }
        entry.raw_value = {blob.begin() + static_cast<std::ptrdiff_t>(value_start),
                           blob.begin() + static_cast<std::ptrdiff_t>(offset)};
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<std::uint8_t> encode_entity_metadata(const std::vector<MetadataEntry>& entries) {
    std::vector<std::uint8_t> out;
    for (const auto& entry : entries) {
        codec::write_ubyte(out, entry.index);
        append_var_int(out, static_cast<std::int32_t>(entry.type));
        out.insert(out.end(), entry.raw_value.begin(), entry.raw_value.end());
    }
    codec::write_ubyte(out, 0xFF);
    return out;
}

} // namespace kprotocol
