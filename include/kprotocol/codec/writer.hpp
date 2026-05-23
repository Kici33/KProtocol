#pragma once

#include "kprotocol/codec/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kprotocol::codec {

enum class WriteError : std::uint8_t {
    ok = 0,
    length_exceeds_limit,       // string / byte-array length above the configured cap
    value_out_of_range,         // value would not fit in the target encoding
};

// Append-only writer over a caller-owned std::vector<uint8_t>.
//
// Design contract:
//   - The Writer does NOT own its buffer; the caller passes a reference and
//     keeps it alive for the writer's lifetime.
//   - On overflow / invalid input, error() becomes non-ok. Subsequent writes
//     are no-ops; the partial output may stay in the buffer, so the caller
//     should treat any failed encode as "discard and retry".
//   - This type does not throw; the throwing convenience helpers in
//     codec.hpp call it and raise EncodeError on a non-ok terminal state.
class Writer {
public:
    explicit Writer(std::vector<std::uint8_t>& out) noexcept : out_(out) {}

    [[nodiscard]] WriteError error() const noexcept { return error_; }
    [[nodiscard]] bool ok() const noexcept { return error_ == WriteError::ok; }
    [[nodiscard]] std::size_t bytes_written() const noexcept { return written_; }

    void write_var_int(std::int32_t value) noexcept;
    void write_var_long(std::int64_t value) noexcept;

    void write_u8(std::uint8_t value) noexcept;
    void write_i8(std::int8_t value) noexcept;
    void write_u16_be(std::uint16_t value) noexcept;
    void write_i16_be(std::int16_t value) noexcept;
    void write_u32_be(std::uint32_t value) noexcept;
    void write_i32_be(std::int32_t value) noexcept;
    void write_u64_be(std::uint64_t value) noexcept;
    void write_i64_be(std::int64_t value) noexcept;
    void write_f32_be(float value) noexcept;
    void write_f64_be(double value) noexcept;

    void write_bool(bool value) noexcept;

    // VarInt-length-prefixed string with caller-supplied upper bound.
    void write_string(const std::string& value,
                      std::int32_t max_len = limits::max_string_length) noexcept;

    // VarInt-length-prefixed byte slice.
    void write_bytes(std::span<const std::uint8_t> value,
                     std::int32_t max_len = limits::max_packet_length) noexcept;

    // Raw (unprefixed) byte append.
    void write_raw(std::span<const std::uint8_t> value) noexcept;

private:
    void poison(WriteError e) noexcept {
        if (error_ == WriteError::ok) error_ = e;
    }

    std::vector<std::uint8_t>& out_;
    std::size_t written_ = 0;
    WriteError error_ = WriteError::ok;
};

// Return the encoded byte size of a VarInt for a given value.
// Equivalent to running write_var_int into a sink and counting bytes,
// but with a closed-form expression (constant time, no allocation).
[[nodiscard]] constexpr std::int32_t var_int_size(std::int32_t value) noexcept {
    const auto u = static_cast<std::uint32_t>(value);
    if ((u & 0xFFFFFF80U) == 0U) return 1;
    if ((u & 0xFFFFC000U) == 0U) return 2;
    if ((u & 0xFFE00000U) == 0U) return 3;
    if ((u & 0xF0000000U) == 0U) return 4;
    return 5;
}

} // namespace kprotocol::codec
