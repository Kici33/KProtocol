#include "kprotocol/types.hpp"
#include "kprotocol/codec.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

namespace kprotocol {

namespace {

constexpr std::size_t kMaxNbtDepth = 64;
constexpr std::int32_t kMaxNbtElements = 1'000'000;

int hex_nibble(const char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

char hex_char(const std::uint8_t nibble) noexcept {
    constexpr char kHex[] = "0123456789abcdef";
    return kHex[nibble & 0x0F];
}

std::uint8_t to_wire_type(const NBTTagType type) {
    return static_cast<std::uint8_t>(type);
}

NBTTagType nbt_type_from_wire(const std::uint8_t type) {
    if (type > to_wire_type(NBTTagType::long_array)) {
        throw std::runtime_error("Unsupported NBT tag type");
    }
    return static_cast<NBTTagType>(type);
}

void ensure_available(std::span<const std::uint8_t> input, std::size_t offset, std::size_t count, const char* what) {
    if (count > input.size() - offset) {
        throw std::runtime_error(std::string("Truncated NBT ") + what);
    }
}

std::uint16_t read_nbt_u16(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 2, "string length");
    const auto value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(input[offset]) << 8U) |
        static_cast<std::uint16_t>(input[offset + 1]));
    offset += 2;
    return value;
}

void write_nbt_string(std::vector<std::uint8_t>& out, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("NBT string exceeds u16 length limit");
    }
    const auto length = static_cast<std::uint16_t>(value.size());
    codec::write_u16(out, length);
    out.insert(out.end(), value.begin(), value.end());
}

std::string read_nbt_string(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto length = read_nbt_u16(input, offset);
    ensure_available(input, offset, length, "string");
    const auto* begin = reinterpret_cast<const char*>(input.data() + offset);
    std::string value(begin, begin + length);
    offset += length;
    return value;
}

void validate_count(const std::int32_t count, const char* what) {
    if (count < 0) {
        throw std::runtime_error(std::string("Negative NBT ") + what + " length");
    }
    if (count > kMaxNbtElements) {
        throw std::runtime_error(std::string("NBT ") + what + " length exceeds limit");
    }
}

NBTValue read_nbt_payload(std::span<const std::uint8_t> input,
                          std::size_t& offset,
                          NBTTagType type,
                          std::size_t depth);
void write_nbt_payload(std::vector<std::uint8_t>& out, const NBTValue& value);

NBTValue read_nbt_payload(std::span<const std::uint8_t> input,
                          std::size_t& offset,
                          const NBTTagType type,
                          const std::size_t depth) {
    if (depth > kMaxNbtDepth) {
        throw std::runtime_error("NBT nesting exceeds limit");
    }

    NBTValue value;
    value.type = type;
    switch (type) {
    case NBTTagType::end:
        return value;
    case NBTTagType::byte:
        value.byte_value = codec::read_byte(input, offset);
        return value;
    case NBTTagType::short_:
        value.short_value = codec::read_short(input, offset);
        return value;
    case NBTTagType::int_:
        value.int_value = codec::read_int(input, offset);
        return value;
    case NBTTagType::long_:
        value.long_value = codec::read_long(input, offset);
        return value;
    case NBTTagType::float_:
        value.float_value = codec::read_float(input, offset);
        return value;
    case NBTTagType::double_:
        value.double_value = codec::read_double(input, offset);
        return value;
    case NBTTagType::byte_array: {
        const auto count = codec::read_int(input, offset);
        validate_count(count, "byte array");
        ensure_available(input, offset, static_cast<std::size_t>(count), "byte array");
        value.byte_array.assign(
            input.begin() + static_cast<std::ptrdiff_t>(offset),
            input.begin() + static_cast<std::ptrdiff_t>(offset + static_cast<std::size_t>(count)));
        offset += static_cast<std::size_t>(count);
        return value;
    }
    case NBTTagType::string:
        value.string_value = read_nbt_string(input, offset);
        return value;
    case NBTTagType::list: {
        ensure_available(input, offset, 1, "list type");
        value.list_element_type = nbt_type_from_wire(input[offset++]);
        const auto count = codec::read_int(input, offset);
        validate_count(count, "list");
        if (value.list_element_type == NBTTagType::end && count > 0) {
            throw std::runtime_error("NBT list with TAG_End elements cannot be non-empty");
        }
        value.list_values.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i) {
            value.list_values.push_back(read_nbt_payload(input, offset, value.list_element_type, depth + 1));
        }
        return value;
    }
    case NBTTagType::compound:
        while (true) {
            ensure_available(input, offset, 1, "compound child type");
            const auto child_type = nbt_type_from_wire(input[offset++]);
            if (child_type == NBTTagType::end) {
                return value;
            }
            value.compound_names.push_back(read_nbt_string(input, offset));
            value.compound_values.push_back(read_nbt_payload(input, offset, child_type, depth + 1));
        }
    case NBTTagType::int_array: {
        const auto count = codec::read_int(input, offset);
        validate_count(count, "int array");
        value.int_array.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i) {
            value.int_array.push_back(codec::read_int(input, offset));
        }
        return value;
    }
    case NBTTagType::long_array: {
        const auto count = codec::read_int(input, offset);
        validate_count(count, "long array");
        value.long_array.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i) {
            value.long_array.push_back(codec::read_long(input, offset));
        }
        return value;
    }
    }
    throw std::runtime_error("Unsupported NBT tag type");
}

void write_nbt_payload(std::vector<std::uint8_t>& out, const NBTValue& value) {
    switch (value.type) {
    case NBTTagType::end:
        return;
    case NBTTagType::byte:
        codec::write_byte(out, value.byte_value);
        return;
    case NBTTagType::short_:
        codec::write_short(out, value.short_value);
        return;
    case NBTTagType::int_:
        codec::write_int(out, value.int_value);
        return;
    case NBTTagType::long_:
        codec::write_long(out, value.long_value);
        return;
    case NBTTagType::float_:
        codec::write_float(out, value.float_value);
        return;
    case NBTTagType::double_:
        codec::write_double(out, value.double_value);
        return;
    case NBTTagType::byte_array:
        codec::write_int(out, static_cast<std::int32_t>(value.byte_array.size()));
        out.insert(out.end(), value.byte_array.begin(), value.byte_array.end());
        return;
    case NBTTagType::string:
        write_nbt_string(out, value.string_value);
        return;
    case NBTTagType::list:
        if (value.list_element_type == NBTTagType::end && !value.list_values.empty()) {
            throw std::runtime_error("NBT list with TAG_End elements cannot be non-empty");
        }
        out.push_back(to_wire_type(value.list_element_type));
        codec::write_int(out, static_cast<std::int32_t>(value.list_values.size()));
        for (const auto& item : value.list_values) {
            if (item.type != value.list_element_type) {
                throw std::runtime_error("NBT list contains mixed tag types");
            }
            write_nbt_payload(out, item);
        }
        return;
    case NBTTagType::compound:
        if (value.compound_names.size() != value.compound_values.size()) {
            throw std::runtime_error("NBT compound name/value count mismatch");
        }
        for (std::size_t i = 0; i < value.compound_values.size(); ++i) {
            const auto& child = value.compound_values[i];
            if (child.type == NBTTagType::end) {
                throw std::runtime_error("NBT compound child cannot be TAG_End");
            }
            out.push_back(to_wire_type(child.type));
            write_nbt_string(out, value.compound_names[i]);
            write_nbt_payload(out, child);
        }
        out.push_back(to_wire_type(NBTTagType::end));
        return;
    case NBTTagType::int_array:
        codec::write_int(out, static_cast<std::int32_t>(value.int_array.size()));
        for (const auto item : value.int_array) {
            codec::write_int(out, item);
        }
        return;
    case NBTTagType::long_array:
        codec::write_int(out, static_cast<std::int32_t>(value.long_array.size()));
        for (const auto item : value.long_array) {
            codec::write_long(out, item);
        }
        return;
    }
    throw std::runtime_error("Unsupported NBT tag type");
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
    if (!nbt.data.empty()) {
        std::size_t offset = 0;
        ensure_available(nbt.data, offset, 1, "root tag type");
        const auto type = nbt_type_from_wire(nbt.data[offset++]);
        (void)read_nbt_payload(nbt.data, offset, type, 0);
        if (offset != nbt.data.size()) {
            throw std::runtime_error("NBT blob has trailing bytes");
        }
    }
    out.insert(out.end(), nbt.data.begin(), nbt.data.end());
}

NBTBlob read_nbt(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto start = offset;
    ensure_available(input, offset, 1, "root tag type");
    const auto type = nbt_type_from_wire(input[offset++]);
    (void)read_nbt_payload(input, offset, type, 0);
    NBTBlob blob;
    blob.data.insert(
        blob.data.end(),
        input.begin() + static_cast<std::ptrdiff_t>(start),
        input.begin() + static_cast<std::ptrdiff_t>(offset));
    return blob;
}

void write_named_nbt(std::vector<std::uint8_t>& out, const NBTNamedTag& tag) {
    if (tag.value.type == NBTTagType::end) {
        out.push_back(to_wire_type(NBTTagType::end));
        return;
    }
    out.push_back(to_wire_type(tag.value.type));
    write_nbt_string(out, tag.name);
    write_nbt_payload(out, tag.value);
}

NBTNamedTag read_named_nbt(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 1, "root tag type");
    const auto type = nbt_type_from_wire(input[offset++]);
    if (type == NBTTagType::end) {
        return {};
    }
    NBTNamedTag tag;
    tag.name = read_nbt_string(input, offset);
    tag.value = read_nbt_payload(input, offset, type, 0);
    return tag;
}

NBTBlob encode_named_nbt(const NBTNamedTag& tag) {
    NBTBlob blob;
    write_named_nbt(blob.data, tag);
    return blob;
}

NBTNamedTag decode_named_nbt(const NBTBlob& blob) {
    std::size_t offset = 0;
    const auto tag = read_named_nbt(blob.data, offset);
    if (offset != blob.data.size()) {
        throw std::runtime_error("NBT blob has trailing bytes");
    }
    return tag;
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
    return read_nbt(input, offset);
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

UUID UUID::from_string(std::string_view text) {
    std::array<char, 32> raw{};
    std::size_t raw_size = 0;
    for (const char c : text) {
        if (c == '-') {
            continue;
        }
        if (raw_size >= raw.size()) {
            throw std::invalid_argument("UUID has too many hex digits");
        }
        raw[raw_size++] = c;
    }
    if (raw_size != raw.size()) {
        throw std::invalid_argument("UUID must contain 32 hex digits");
    }

    UUID uuid;
    for (std::size_t i = 0; i < uuid.bytes.size(); ++i) {
        const int high = hex_nibble(raw[i * 2]);
        const int low = hex_nibble(raw[i * 2 + 1]);
        if (high < 0 || low < 0) {
            throw std::invalid_argument("UUID contains non-hex digit");
        }
        uuid.bytes[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return uuid;
}

std::string UUID::to_string() const {
    std::string output;
    output.reserve(36U);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4U || i == 6U || i == 8U || i == 10U) {
            output.push_back('-');
        }
        output.push_back(hex_char(static_cast<std::uint8_t>(bytes[i] >> 4)));
        output.push_back(hex_char(bytes[i]));
    }
    return output;
}

} // namespace kprotocol
