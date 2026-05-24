#include "kprotocol/types.hpp"
#include "kprotocol/codec.hpp"

#include <cstring>
#include <stdexcept>

namespace kprotocol {

namespace {

// Minimal NBT tag skip/read for anonOptionalNbt inside slots. Supports the
// tag kinds commonly seen on item stacks (compound + nested primitives).
void skip_nbt_payload(std::span<const std::uint8_t> input, std::size_t& offset, std::uint8_t tag_type);

void skip_nbt_value(std::span<const std::uint8_t> input, std::size_t& offset, std::uint8_t tag_type) {
    switch (tag_type) {
    case 0: // TAG_End
        return;
    case 1: // TAG_Byte
        if (offset + 1 > input.size()) throw std::runtime_error("Truncated NBT byte");
        offset += 1;
        return;
    case 2: // TAG_Short
        if (offset + 2 > input.size()) throw std::runtime_error("Truncated NBT short");
        offset += 2;
        return;
    case 3: // TAG_Int
        if (offset + 4 > input.size()) throw std::runtime_error("Truncated NBT int");
        offset += 4;
        return;
    case 4: // TAG_Long
        if (offset + 8 > input.size()) throw std::runtime_error("Truncated NBT long");
        offset += 8;
        return;
    case 5: // TAG_Float
        if (offset + 4 > input.size()) throw std::runtime_error("Truncated NBT float");
        offset += 4;
        return;
    case 6: // TAG_Double
        if (offset + 8 > input.size()) throw std::runtime_error("Truncated NBT double");
        offset += 8;
        return;
    case 7: { // TAG_Byte_Array
        const auto len = codec::read_int(input, offset);
        if (len < 0 || static_cast<std::size_t>(len) > input.size() - offset) {
            throw std::runtime_error("Truncated NBT byte array");
        }
        offset += static_cast<std::size_t>(len);
        return;
    }
    case 8: { // TAG_String
        (void)codec::read_string(input, offset);
        return;
    }
    case 9: { // TAG_List
        if (offset + 1 > input.size()) throw std::runtime_error("Truncated NBT list");
        const auto element_type = input[offset++];
        const auto len = codec::read_int(input, offset);
        if (len < 0) throw std::runtime_error("Negative NBT list length");
        for (std::int32_t i = 0; i < len; ++i) {
            skip_nbt_value(input, offset, element_type);
        }
        return;
    }
    case 10: // TAG_Compound
        skip_nbt_payload(input, offset, tag_type);
        return;
    case 11: { // TAG_Int_Array
        const auto len = codec::read_int(input, offset);
        if (len < 0 || static_cast<std::size_t>(len) * 4U > input.size() - offset) {
            throw std::runtime_error("Truncated NBT int array");
        }
        offset += static_cast<std::size_t>(len) * 4U;
        return;
    }
    case 12: { // TAG_Long_Array
        const auto len = codec::read_int(input, offset);
        if (len < 0 || static_cast<std::size_t>(len) * 8U > input.size() - offset) {
            throw std::runtime_error("Truncated NBT long array");
        }
        offset += static_cast<std::size_t>(len) * 8U;
        return;
    }
    default:
        throw std::runtime_error("Unsupported NBT tag type");
    }
}

void skip_nbt_payload(std::span<const std::uint8_t> input, std::size_t& offset, std::uint8_t tag_type) {
    if (tag_type != 10) {
        skip_nbt_value(input, offset, tag_type);
        return;
    }
    while (offset < input.size()) {
        if (offset + 1 > input.size()) throw std::runtime_error("Truncated NBT compound");
        const auto child_type = input[offset++];
        if (child_type == 0) {
            return;
        }
        (void)codec::read_string(input, offset);
        skip_nbt_value(input, offset, child_type);
    }
    throw std::runtime_error("Unterminated NBT compound");
}

NBTBlob read_nbt_tag(std::span<const std::uint8_t> input, std::size_t& offset) {
    if (offset >= input.size()) {
        throw std::runtime_error("Unexpected end reading NBT tag");
    }
    const auto start = offset;
    const auto tag_type = input[offset++];
    skip_nbt_payload(input, offset, tag_type);
    NBTBlob blob;
    blob.data.insert(blob.data.end(), input.begin() + static_cast<std::ptrdiff_t>(start),
                     input.begin() + static_cast<std::ptrdiff_t>(offset));
    return blob;
}

} // namespace

namespace types {

void write_long(std::vector<std::uint8_t>& out, std::int64_t value) {
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i*8)) & 0xFF));
    }
}

std::int64_t read_long(std::span<const std::uint8_t> input, std::size_t& offset) {
    if (offset + 8 > input.size()) {
        throw std::runtime_error("Unexpected end of input reading long");
    }
    std::int64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<std::int64_t>(input[offset + i]);
    }
    offset += 8;
    return value;
}

void write_position(std::vector<std::uint8_t>& out, const Position& p) {
    // pack: ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF)
    const std::int64_t x = static_cast<std::int64_t>(p.x) & 0x3FFFFFFLL;
    const std::int64_t z = static_cast<std::int64_t>(p.z) & 0x3FFFFFFLL;
    const std::int64_t y = static_cast<std::int64_t>(p.y) & 0xFFFLL;
    const std::int64_t val = (x << 38) | (z << 12) | y;
    write_long(out, val);
}

Position read_position(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto val = read_long(input, offset);
    Position p;
    p.x = static_cast<std::int32_t>(val >> 38);
    p.y = static_cast<std::int32_t>((val << 52) >> 52);
    p.z = static_cast<std::int32_t>((val << 26) >> 38);
    return p;
}

void write_uuid(std::vector<std::uint8_t>& out, const UUID& u) {
    out.insert(out.end(), u.bytes.begin(), u.bytes.end());
}

UUID read_uuid(std::span<const std::uint8_t> input, std::size_t& offset) {
    if (offset + 16 > input.size()) {
        throw std::runtime_error("Unexpected end reading UUID");
    }
    UUID u;
    std::memcpy(u.bytes.data(), input.data() + offset, 16);
    offset += 16;
    return u;
}

void write_nbt(std::vector<std::uint8_t>& out, const NBTBlob& nbt) {
    // NBT in many contexts is length-prefixed (VarInt) or raw; here append raw bytes
    out.insert(out.end(), nbt.data.begin(), nbt.data.end());
}

NBTBlob read_nbt(std::span<const std::uint8_t> input, std::size_t& offset) {
    // No length information here — caller must manage boundaries. We'll return remaining bytes from offset.
    NBTBlob b;
    if (offset < input.size()) {
        b.data.insert(b.data.end(), input.begin() + static_cast<std::ptrdiff_t>(offset), input.end());
        offset = input.size();
    }
    return b;
}

void write_optional_nbt(std::vector<std::uint8_t>& out, const NBTBlob& nbt) {
    if (nbt.data.empty()) {
        out.push_back(0x00);
        return;
    }
    out.insert(out.end(), nbt.data.begin(), nbt.data.end());
}

NBTBlob read_optional_nbt(std::span<const std::uint8_t> input, std::size_t& offset) {
    if (offset >= input.size()) {
        throw std::runtime_error("Unexpected end reading optional NBT");
    }
    if (input[offset] == 0x00) {
        ++offset;
        return {};
    }
    return read_nbt_tag(input, offset);
}

void write_slot(std::vector<std::uint8_t>& out, const Slot& slot) {
    codec::write_bool(out, slot.present);
    if (!slot.present) {
        return;
    }
    codec::write_var_int(out, slot.item_id);
    codec::write_byte(out, static_cast<std::int8_t>(slot.count));
    write_optional_nbt(out, slot.nbt);
}

Slot read_slot(std::span<const std::uint8_t> input, std::size_t& offset) {
    Slot slot;
    slot.present = codec::read_bool(input, offset);
    if (!slot.present) {
        return slot;
    }
    slot.item_id = codec::read_var_int(input, offset);
    slot.count = codec::read_byte(input, offset);
    slot.nbt = read_optional_nbt(input, offset);
    return slot;
}

// Slot encoding: present(bool) + (if present) var_int item_id + byte count + optional NBT
std::vector<std::uint8_t> slot_to_bytes(const Slot& slot) {
    std::vector<std::uint8_t> out;
    write_slot(out, slot);
    return out;
}

Slot slot_from_bytes(const std::vector<std::uint8_t>& blob) {
    std::size_t offset = 0;
    const std::span<const std::uint8_t> input(blob.data(), blob.size());
    return read_slot(input, offset);
}

} // namespace types

std::int64_t Position::to_long() const noexcept {
    // pack similar to write_position
    const std::int64_t px = static_cast<std::int64_t>(this->x) & 0x3FFFFFFLL;
    const std::int64_t pz = static_cast<std::int64_t>(this->z) & 0x3FFFFFFLL;
    const std::int64_t py = static_cast<std::int64_t>(this->y) & 0xFFFLL;
    return (px << 38) | (pz << 12) | py;
}

Position Position::from_long(std::int64_t v) noexcept {
    Position p;
    p.x = static_cast<std::int32_t>(v >> 38);
    p.y = static_cast<std::int32_t>((v << 52) >> 52);
    p.z = static_cast<std::int32_t>((v << 26) >> 38);
    return p;
}

UUID UUID::from_bytes(std::span<const std::uint8_t> data) {
    UUID u;
    if (data.size() >= 16) {
        std::memcpy(u.bytes.data(), data.data(), 16);
    }
    return u;
}

} // namespace kprotocol
