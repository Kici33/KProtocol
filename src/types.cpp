#include "kprotocol/types.hpp"
#include "kprotocol/codec.hpp"

#include <cstring>
#include <stdexcept>

namespace kprotocol {

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

// Slot encoding: present(bool) + (if present) var_int item_id + byte count + NBT (as bytes with var_int length)
std::vector<std::uint8_t> slot_to_bytes(const Slot& slot) {
    std::vector<std::uint8_t> out;
    // present flag
    out.push_back(static_cast<std::uint8_t>(slot.present ? 0x01 : 0x00));
    if (!slot.present) return out;
    // item id as VarInt
    codec::write_var_int(out, slot.item_id);
    // count as signed byte
    out.push_back(static_cast<std::uint8_t>(slot.count & 0xFF));
    // nbt: write length-prefixed bytes (VarInt length + raw bytes)
    codec::write_var_int(out, static_cast<std::int32_t>(slot.nbt.data.size()));
    out.insert(out.end(), slot.nbt.data.begin(), slot.nbt.data.end());
    return out;
}

Slot slot_from_bytes(const std::vector<std::uint8_t>& blob) {
    Slot s;
    std::size_t offset = 0;
    const std::span<const std::uint8_t> input(blob.data(), blob.size());
    if (input.size() == 0) {
        s.present = false;
        return s;
    }
    s.present = input[offset++] != 0;
    if (!s.present) return s;
    // read var_int item id
    s.item_id = codec::read_var_int(input, offset);
    // read count (signed byte)
    if (offset >= input.size()) throw std::runtime_error("Unexpected end reading slot count");
    s.count = static_cast<std::int32_t>(static_cast<int8_t>(input[offset++]));
    // read nbt length
    const auto nbt_len = codec::read_var_int(input, offset);
    if (nbt_len < 0) throw std::runtime_error("Negative NBT length");
    if (static_cast<std::size_t>(nbt_len) > input.size() - offset) throw std::runtime_error("Truncated NBT in slot");
    s.nbt.data.insert(s.nbt.data.end(), input.begin() + static_cast<std::ptrdiff_t>(offset), input.begin() + static_cast<std::ptrdiff_t>(offset + static_cast<std::size_t>(nbt_len)));
    offset += static_cast<std::size_t>(nbt_len);
    return s;
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
