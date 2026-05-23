#pragma once

#include "kprotocol/codec/limits.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace kprotocol::codec {

// Classification of a Reader failure. The first error "poisons" the Reader;
// subsequent reads short-circuit and return sentinel values without further
// touching the underlying buffer.
enum class ReadError : std::uint8_t {
    ok = 0,
    truncated,                  // buffer ended mid-value
    overlong_varint,            // VarInt / VarLong continuation exceeded its max byte count
    length_exceeds_limit,       // length-prefixed value declared a length above the configured cap
    negative_length,            // length-prefixed value declared a negative length
};

// Bounds-checked, non-throwing binary reader over a borrowed byte span.
//
// Design contract:
//   - Every read_* method returns a sentinel (zero, empty string, empty span)
//     on failure AND sets error() to a non-ok value.
//   - Once poisoned, subsequent reads are no-ops; the cursor does not advance.
//   - The Reader does NOT own its buffer; the caller must keep the span alive.
//   - Reads do not throw. This is intentional: decoders driven by network I/O
//     (in particular Wave 3's coroutine sessions) must be able to react to
//     malformed input without exception-driven control flow.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) noexcept
        : data_(data) {}

    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t remaining_bytes() const noexcept {
        return cursor_ <= data_.size() ? data_.size() - cursor_ : 0U;
    }
    [[nodiscard]] bool has_remaining(std::size_t n) const noexcept {
        return remaining_bytes() >= n;
    }

    [[nodiscard]] ReadError error() const noexcept { return error_; }
    [[nodiscard]] bool ok() const noexcept { return error_ == ReadError::ok; }

    // VarInt / VarLong with overlong-detection.
    [[nodiscard]] std::int32_t read_var_int() noexcept;
    [[nodiscard]] std::int64_t read_var_long() noexcept;

    // Fixed-width primitives (big-endian).
    [[nodiscard]] std::uint8_t  read_u8() noexcept;
    [[nodiscard]] std::int8_t   read_i8() noexcept;
    [[nodiscard]] std::uint16_t read_u16_be() noexcept;
    [[nodiscard]] std::int16_t  read_i16_be() noexcept;
    [[nodiscard]] std::uint32_t read_u32_be() noexcept;
    [[nodiscard]] std::int32_t  read_i32_be() noexcept;
    [[nodiscard]] std::uint64_t read_u64_be() noexcept;
    [[nodiscard]] std::int64_t  read_i64_be() noexcept;
    [[nodiscard]] float         read_f32_be() noexcept;
    [[nodiscard]] double        read_f64_be() noexcept;

    // Boolean (single byte; any non-zero is true).
    [[nodiscard]] bool read_bool() noexcept;

    // VarInt-length-prefixed string with caller-supplied upper bound.
    [[nodiscard]] std::string read_string(std::int32_t max_len = limits::max_string_length) noexcept;

    // VarInt-length-prefixed byte slice (zero-copy view into the underlying span).
    [[nodiscard]] std::span<const std::uint8_t>
    read_bytes(std::int32_t max_len = limits::max_packet_length) noexcept;

private:
    void poison(ReadError e) noexcept {
        if (error_ == ReadError::ok) error_ = e;
    }

    std::span<const std::uint8_t> data_;
    std::size_t cursor_ = 0;
    ReadError error_ = ReadError::ok;
};

} // namespace kprotocol::codec
