#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kprotocol {

struct BlockState {
    std::int32_t id{}; // canonical block state id; mapping tools should translate per-version
};

struct Position {
    std::int32_t x{}; // block coordinates
    std::int32_t y{};
    std::int32_t z{};

    // pack/unpack to 64-bit Java position value
    std::int64_t to_long() const noexcept;
    static Position from_long(std::int64_t v) noexcept;

    friend bool operator==(const Position&, const Position&) noexcept = default;
};

struct UUID {
    std::array<std::uint8_t, 16> bytes{};
    static UUID from_bytes(std::span<const std::uint8_t> data);
    static UUID from_string(std::string_view text);
    [[nodiscard]] std::string to_string() const;

    friend bool operator==(const UUID&, const UUID&) noexcept = default;
};

struct NBTBlob {
    std::vector<std::uint8_t> data;

    friend bool operator==(const NBTBlob&, const NBTBlob&) = default;
};

enum class NBTTagType : std::uint8_t {
    end = 0,
    byte = 1,
    short_ = 2,
    int_ = 3,
    long_ = 4,
    float_ = 5,
    double_ = 6,
    byte_array = 7,
    string = 8,
    list = 9,
    compound = 10,
    int_array = 11,
    long_array = 12,
};

struct NBTValue {
    NBTTagType type{NBTTagType::end};
    std::int8_t byte_value{};
    std::int16_t short_value{};
    std::int32_t int_value{};
    std::int64_t long_value{};
    float float_value{};
    double double_value{};
    std::vector<std::uint8_t> byte_array;
    std::string string_value;
    NBTTagType list_element_type{NBTTagType::end};
    std::vector<NBTValue> list_values;
    std::vector<std::string> compound_names;
    std::vector<NBTValue> compound_values;
    std::vector<std::int32_t> int_array;
    std::vector<std::int64_t> long_array;

    friend bool operator==(const NBTValue&, const NBTValue&) = default;
};

struct NBTNamedTag {
    std::string name;
    NBTValue value;

    friend bool operator==(const NBTNamedTag&, const NBTNamedTag&) = default;
};

// Simple encoding/decoding helpers for common composite types
namespace types {

// Slot moved into types namespace for consistency with tests and generated code
struct Slot {
    bool present = false;
    std::int32_t item_id = 0; // protocol item id (var_int in many versions)
    std::int32_t count = 0;   // item count (signed byte in older protocols)
    NBTBlob nbt;              // optional NBT payload
};

void write_long(std::vector<std::uint8_t>& out, std::int64_t value);
std::int64_t read_long(std::span<const std::uint8_t> input, std::size_t& offset);

void write_position(std::vector<std::uint8_t>& out, const Position& p);
Position read_position(std::span<const std::uint8_t> input, std::size_t& offset);

void write_uuid(std::vector<std::uint8_t>& out, const UUID& u);
UUID read_uuid(std::span<const std::uint8_t> input, std::size_t& offset);

void write_nbt(std::vector<std::uint8_t>& out, const NBTBlob& nbt);
NBTBlob read_nbt(std::span<const std::uint8_t> input, std::size_t& offset);
void write_named_nbt(std::vector<std::uint8_t>& out, const NBTNamedTag& tag);
NBTNamedTag read_named_nbt(std::span<const std::uint8_t> input, std::size_t& offset);
NBTBlob encode_named_nbt(const NBTNamedTag& tag);
NBTNamedTag decode_named_nbt(const NBTBlob& blob);

void write_slot(std::vector<std::uint8_t>& out, const Slot& slot);
Slot read_slot(std::span<const std::uint8_t> input, std::size_t& offset);

void write_optional_nbt(std::vector<std::uint8_t>& out, const NBTBlob& nbt);
NBTBlob read_optional_nbt(std::span<const std::uint8_t> input, std::size_t& offset);

// Legacy helpers kept for codec_tests.
std::vector<std::uint8_t> slot_to_bytes(const Slot& slot);
Slot slot_from_bytes(const std::vector<std::uint8_t>& blob);

} // namespace types

} // namespace kprotocol
