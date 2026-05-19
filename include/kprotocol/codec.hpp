#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kprotocol::codec {

struct EncodedFrame {
    std::int32_t packet_id{};
    std::vector<std::uint8_t> payload;
};

// ===== Primitive Type Encoders (VarInt/VarLong family) =====
void write_var_int(std::vector<std::uint8_t>& out, std::int32_t value);
void write_var_long(std::vector<std::uint8_t>& out, std::int64_t value);

// ===== Fixed-Size Integer Types =====
void write_byte(std::vector<std::uint8_t>& out, std::int8_t value);
void write_ubyte(std::vector<std::uint8_t>& out, std::uint8_t value);
void write_short(std::vector<std::uint8_t>& out, std::int16_t value);
void write_ushort(std::vector<std::uint8_t>& out, std::uint16_t value);
void write_int(std::vector<std::uint8_t>& out, std::int32_t value);
void write_uint(std::vector<std::uint8_t>& out, std::uint32_t value);
void write_long(std::vector<std::uint8_t>& out, std::int64_t value);
void write_ulong(std::vector<std::uint8_t>& out, std::uint64_t value);

// ===== Floating-Point Types =====
void write_float(std::vector<std::uint8_t>& out, float value);
void write_double(std::vector<std::uint8_t>& out, double value);

// ===== Boolean & Byte-Array Types =====
void write_bool(std::vector<std::uint8_t>& out, bool value);
void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value);  // network byte order (big-endian)
void write_string(std::vector<std::uint8_t>& out, const std::string& value);
void write_bytes(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value);
void write_byte_array(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value);

// ===== Angle & Rotation =====
void write_angle(std::vector<std::uint8_t>& out, float yaw);  // wraps to 0.0-1.0 then encodes as single byte
void write_rotation(std::vector<std::uint8_t>& out, float yaw, float pitch);  // two bytes

// ===== Array Types =====
void write_var_int_array(std::vector<std::uint8_t>& out, std::span<const std::int32_t> values);
void write_var_long_array(std::vector<std::uint8_t>& out, std::span<const std::int64_t> values);

// ===== Primitive Type Decoders =====
std::int32_t read_var_int(std::span<const std::uint8_t> input, std::size_t& offset);
std::int64_t read_var_long(std::span<const std::uint8_t> input, std::size_t& offset);

// ===== Fixed-Size Integer Decoders =====
std::int8_t read_byte(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint8_t read_ubyte(std::span<const std::uint8_t> input, std::size_t& offset);
std::int16_t read_short(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint16_t read_ushort(std::span<const std::uint8_t> input, std::size_t& offset);
std::int32_t read_int(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint32_t read_uint(std::span<const std::uint8_t> input, std::size_t& offset);
std::int64_t read_long(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint64_t read_ulong(std::span<const std::uint8_t> input, std::size_t& offset);

// ===== Floating-Point Decoders =====
float read_float(std::span<const std::uint8_t> input, std::size_t& offset);
double read_double(std::span<const std::uint8_t> input, std::size_t& offset);

// ===== Boolean & Byte-Array Decoders =====
bool read_bool(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint16_t read_u16(std::span<const std::uint8_t> input, std::size_t& offset);
std::string read_string(std::span<const std::uint8_t> input, std::size_t& offset);
std::vector<std::uint8_t> read_bytes(std::span<const std::uint8_t> input, std::size_t& offset);
std::vector<std::uint8_t> read_byte_array(std::span<const std::uint8_t> input, std::size_t& offset);

// ===== Angle & Rotation Decoders =====
float read_angle(std::span<const std::uint8_t> input, std::size_t& offset);  // single byte -> 0.0-1.0
std::pair<float, float> read_rotation(std::span<const std::uint8_t> input, std::size_t& offset);  // two bytes -> (yaw, pitch)

// ===== Array Type Decoders =====
std::vector<std::int32_t> read_var_int_array(std::span<const std::uint8_t> input, std::size_t& offset);
std::vector<std::int64_t> read_var_long_array(std::span<const std::uint8_t> input, std::size_t& offset);

// ===== Frame Encoding/Decoding =====
std::vector<std::uint8_t> encode_frame(std::int32_t packet_id, std::span<const std::uint8_t> payload);
bool try_decode_frame(std::span<const std::uint8_t> input, std::size_t& consumed, EncodedFrame& frame);

// ===== Compression Support =====
/**
 * Compress packet data using zlib deflate algorithm.
 * Returns the compressed data (raw deflate format without zlib header).
 * Throws std::runtime_error if compression fails.
 */
std::vector<std::uint8_t> compress_packet(std::span<const std::uint8_t> data);

/**
 * Decompress zlib-compressed packet data.
 * Returns the decompressed data (original uncompressed payload).
 * Throws std::runtime_error if decompression fails or data is corrupted.
 */
std::vector<std::uint8_t> decompress_packet(std::span<const std::uint8_t> compressed_data);

/**
 * Encode a frame with optional compression.
 * If compressed_threshold is -1 (no compression), uses standard frame encoding.
 * If data_length >= threshold, automatically compresses the payload.
 * Returns the encoded frame ready to send over network.
 */
std::vector<std::uint8_t> encode_frame_compressed(
    std::int32_t packet_id,
    std::span<const std::uint8_t> payload,
    std::int32_t compression_threshold);

/**
 * Decode a frame that may be compressed.
 * Automatically detects and decompresses if needed based on data_length field.
 * Returns true if a complete frame was decoded, false if more data is needed.
 */
bool try_decode_frame_compressed(
    std::span<const std::uint8_t> input,
    std::size_t& consumed,
    std::int32_t compression_threshold,
    EncodedFrame& frame);

} // namespace kprotocol::codec
