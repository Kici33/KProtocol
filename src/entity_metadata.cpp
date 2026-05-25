#include "kprotocol/entity_metadata.hpp"

#include "kprotocol/codec.hpp"

namespace kprotocol {

namespace {

void store_raw(MetadataEntry& entry, std::span<const std::uint8_t> blob, std::size_t start, std::size_t end) {
    entry.raw_value.assign(blob.begin() + static_cast<std::ptrdiff_t>(start),
                           blob.begin() + static_cast<std::ptrdiff_t>(end));
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

        MetadataEntry entry;
        entry.index = index;
        entry.type = static_cast<MetadataType>(codec::read_var_int(blob, offset));
        const std::size_t value_start = offset;

        try {
            switch (entry.type) {
            case MetadataType::byte:
                entry.value = codec::read_byte(blob, offset);
                break;
            case MetadataType::var_int:
                entry.value = codec::read_var_int(blob, offset);
                break;
            case MetadataType::var_long:
                entry.value = codec::read_var_long(blob, offset);
                break;
            case MetadataType::float32:
                entry.value = codec::read_float(blob, offset);
                break;
            case MetadataType::string:
            case MetadataType::component:
            case MetadataType::optional_component:
                entry.value = codec::read_string(blob, offset);
                break;
            case MetadataType::boolean:
                entry.value = codec::read_bool(blob, offset);
                break;
            case MetadataType::block_pos: {
                const auto packed = codec::read_ulong(blob, offset);
                entry.value = Position::from_long(static_cast<std::int64_t>(packed));
                break;
            }
            case MetadataType::optional_block_pos: {
                const bool present = codec::read_bool(blob, offset);
                if (!present) {
                    entry.value = std::optional<Position>{};
                } else {
                    const auto packed = codec::read_ulong(blob, offset);
                    entry.value = std::optional<Position>{Position::from_long(static_cast<std::int64_t>(packed))};
                }
                break;
            }
            case MetadataType::direction:
                entry.value = codec::read_var_int(blob, offset);
                break;
            case MetadataType::optional_uuid: {
                const bool present = codec::read_bool(blob, offset);
                if (!present) {
                    entry.value = std::optional<UUID>{};
                } else {
                    entry.value = std::optional<UUID>{types::read_uuid(blob, offset)};
                }
                break;
            }
            case MetadataType::block_state:
            case MetadataType::optional_block_state:
                entry.value = BlockState{codec::read_var_int(blob, offset)};
                break;
            case MetadataType::item_stack:
                entry.value = types::read_slot(blob, offset);
                break;
            case MetadataType::nbt:
                entry.value = types::read_nbt(blob, offset);
                break;
            default:
                entry.raw_value = {blob.begin() + static_cast<std::ptrdiff_t>(value_start), blob.end()};
                offset = blob.size();
                entries.push_back(std::move(entry));
                return entries;
            }
        } catch (const std::exception&) {
            entry.raw_value = {blob.begin() + static_cast<std::ptrdiff_t>(value_start), blob.end()};
            offset = blob.size();
            entries.push_back(std::move(entry));
            return entries;
        }

        store_raw(entry, blob, value_start, offset);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<std::uint8_t> encode_entity_metadata(const std::vector<MetadataEntry>& entries) {
    std::vector<std::uint8_t> out;
    for (const auto& entry : entries) {
        codec::write_ubyte(out, entry.index);
        codec::write_var_int(out, static_cast<std::int32_t>(entry.type));
        out.insert(out.end(), entry.raw_value.begin(), entry.raw_value.end());
    }
    codec::write_ubyte(out, 0xFF);
    return out;
}

} // namespace kprotocol
