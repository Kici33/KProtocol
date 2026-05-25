#pragma once

#include <array>
#include <cstdint>
#include <span>
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

    friend bool operator==(const UUID&, const UUID&) noexcept = default;
};

struct NBTBlob {
    std::vector<std::uint8_t> data;
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

void write_slot(std::vector<std::uint8_t>& out, const Slot& slot);
Slot read_slot(std::span<const std::uint8_t> input, std::size_t& offset);

void write_optional_nbt(std::vector<std::uint8_t>& out, const NBTBlob& nbt);
NBTBlob read_optional_nbt(std::span<const std::uint8_t> input, std::size_t& offset);

// Legacy helpers kept for codec_tests.
std::vector<std::uint8_t> slot_to_bytes(const Slot& slot);
Slot slot_from_bytes(const std::vector<std::uint8_t>& blob);

} // namespace types

} // namespace kprotocol
