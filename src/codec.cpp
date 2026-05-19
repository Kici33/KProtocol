#include "kprotocol/codec.hpp"

#include <cstring>
#include <stdexcept>
#include <zlib.h>

namespace kprotocol::codec {

namespace {

constexpr std::size_t max_var_int_bytes = 5;
constexpr std::size_t max_var_long_bytes = 10;

bool try_read_var_int(std::span<const std::uint8_t> input, std::size_t start_offset, std::int32_t& value, std::size_t& bytes_read) {
    value = 0;
    bytes_read = 0;
    std::uint32_t result = 0;

    while (bytes_read < max_var_int_bytes) {
        if (start_offset + bytes_read >= input.size()) {
            return false;
        }
        const auto current = input[start_offset + bytes_read];
        result |= static_cast<std::uint32_t>(current & 0x7F) << (7U * bytes_read);
        ++bytes_read;
        if ((current & 0x80U) == 0U) {
            value = static_cast<std::int32_t>(result);
            return true;
        }
    }

    throw std::runtime_error("VarInt exceeds 5 bytes");
}

void ensure_available(const std::span<const std::uint8_t> input, const std::size_t offset, const std::size_t required) {
    if (offset + required > input.size()) {
        throw std::runtime_error("Unexpected end of input");
    }
}

} // namespace

void write_var_int(std::vector<std::uint8_t>& out, std::int32_t value) {
    auto current = static_cast<std::uint32_t>(value);
    do {
        std::uint8_t temp = static_cast<std::uint8_t>(current & 0x7FU);
        current >>= 7U;
        if (current != 0U) {
            temp |= 0x80U;
        }
        out.push_back(temp);
    } while (current != 0U);
}

void write_var_long(std::vector<std::uint8_t>& out, std::int64_t value) {
    auto current = static_cast<std::uint64_t>(value);
    do {
        std::uint8_t temp = static_cast<std::uint8_t>(current & 0x7FU);
        current >>= 7U;
        if (current != 0U) {
            temp |= 0x80U;
        }
        out.push_back(temp);
    } while (current != 0U);
}

void write_bool(std::vector<std::uint8_t>& out, const bool value) {
    out.push_back(static_cast<std::uint8_t>(value ? 0x01 : 0x00));
}

void write_u16(std::vector<std::uint8_t>& out, const std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_string(std::vector<std::uint8_t>& out, const std::string& value) {
    write_var_int(out, static_cast<std::int32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

void write_bytes(std::vector<std::uint8_t>& out, const std::span<const std::uint8_t> value) {
    write_var_int(out, static_cast<std::int32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

std::int32_t read_var_int(const std::span<const std::uint8_t> input, std::size_t& offset) {
    std::int32_t value = 0;
    std::size_t bytes_read = 0;
    if (!try_read_var_int(input, offset, value, bytes_read)) {
        throw std::runtime_error("Incomplete VarInt");
    }
    offset += bytes_read;
    return value;
}

std::int64_t read_var_long(const std::span<const std::uint8_t> input, std::size_t& offset) {
    std::uint64_t result = 0;
    std::size_t num_read = 0;
    while (num_read < max_var_long_bytes) {
        ensure_available(input, offset + num_read, 1);
        const auto read = input[offset + num_read];
        result |= static_cast<std::uint64_t>(read & 0x7FU) << (7U * num_read);
        ++num_read;
        if ((read & 0x80U) == 0U) {
            offset += num_read;
            return static_cast<std::int64_t>(result);
        }
    }
    throw std::runtime_error("VarLong exceeds 10 bytes");
}

bool read_bool(const std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 1);
    return input[offset++] != 0U;
}

std::uint16_t read_u16(const std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 2);
    const auto hi = static_cast<std::uint16_t>(input[offset]);
    const auto lo = static_cast<std::uint16_t>(input[offset + 1]);
    offset += 2;
    return static_cast<std::uint16_t>((hi << 8U) | lo);
}

std::string read_string(const std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto length = read_var_int(input, offset);
    if (length < 0) {
        throw std::runtime_error("Negative string length");
    }
    const auto safe_length = static_cast<std::size_t>(length);
    ensure_available(input, offset, safe_length);
    std::string value;
    value.reserve(safe_length);
    for (std::size_t i = 0; i < safe_length; ++i) {
        value.push_back(static_cast<char>(input[offset + i]));
    }
    offset += safe_length;
    return value;
}

std::vector<std::uint8_t> read_bytes(const std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto length = read_var_int(input, offset);
    if (length < 0) {
        throw std::runtime_error("Negative bytes length");
    }
    const auto safe_length = static_cast<std::size_t>(length);
    ensure_available(input, offset, safe_length);
    std::vector<std::uint8_t> value;
    value.insert(value.end(), input.begin() + static_cast<std::ptrdiff_t>(offset), input.begin() + static_cast<std::ptrdiff_t>(offset + safe_length));
    offset += safe_length;
    return value;
}

std::vector<std::uint8_t> encode_frame(const std::int32_t packet_id, const std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> packet_data;
    write_var_int(packet_data, packet_id);
    packet_data.insert(packet_data.end(), payload.begin(), payload.end());

    std::vector<std::uint8_t> frame;
    write_var_int(frame, static_cast<std::int32_t>(packet_data.size()));
    frame.insert(frame.end(), packet_data.begin(), packet_data.end());
    return frame;
}

bool try_decode_frame(const std::span<const std::uint8_t> input, std::size_t& consumed, EncodedFrame& frame) {
    consumed = 0;
    std::int32_t length = 0;
    std::size_t length_bytes = 0;
    if (!try_read_var_int(input, 0, length, length_bytes)) {
        return false;
    }
    if (length < 0) {
        throw std::runtime_error("Negative frame length");
    }

    const auto payload_size = static_cast<std::size_t>(length);
    if (input.size() < length_bytes + payload_size) {
        return false;
    }

    const auto packet_bytes = input.subspan(length_bytes, payload_size);
    std::size_t offset = 0;
    frame.packet_id = read_var_int(packet_bytes, offset);
    frame.payload.assign(packet_bytes.begin() + static_cast<std::ptrdiff_t>(offset), packet_bytes.end());
    consumed = length_bytes + payload_size;
    return true;
}

// ===== Fixed-Size Integer Types (Big-Endian) =====

void write_byte(std::vector<std::uint8_t>& out, std::int8_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
}

void write_ubyte(std::vector<std::uint8_t>& out, std::uint8_t value) {
    out.push_back(value);
}

void write_short(std::vector<std::uint8_t>& out, std::int16_t value) {
    const auto uval = static_cast<std::uint16_t>(value);
    out.push_back(static_cast<std::uint8_t>((uval >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(uval & 0xFFU));
}

void write_ushort(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_int(std::vector<std::uint8_t>& out, std::int32_t value) {
    const auto uval = static_cast<std::uint32_t>(value);
    out.push_back(static_cast<std::uint8_t>((uval >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((uval >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((uval >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(uval & 0xFFU));
}

void write_uint(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

void write_long(std::vector<std::uint8_t>& out, std::int64_t value) {
    const auto uval = static_cast<std::uint64_t>(value);
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((uval >> (i * 8U)) & 0xFFU));
    }
}

void write_ulong(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 7; i >= 0; --i) {
        out.push_back(static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU));
    }
}

// ===== Floating-Point Types (IEEE-754, Big-Endian) =====

void write_float(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(float));
    write_uint(out, bits);
}

void write_double(std::vector<std::uint8_t>& out, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(double));
    write_ulong(out, bits);
}

// ===== Byte Array Types =====

void write_byte_array(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value) {
    write_var_int(out, static_cast<std::int32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

// ===== Angle & Rotation =====

void write_angle(std::vector<std::uint8_t>& out, float yaw) {
    const float normalized = yaw - static_cast<float>(static_cast<int>(yaw));
    const auto byte_val = static_cast<std::uint8_t>(static_cast<int>(normalized * 256.0f) & 0xFF);
    out.push_back(byte_val);
}

void write_rotation(std::vector<std::uint8_t>& out, float yaw, float pitch) {
    write_angle(out, yaw);
    write_angle(out, pitch);
}

// ===== Array Types =====

void write_var_int_array(std::vector<std::uint8_t>& out, std::span<const std::int32_t> values) {
    write_var_int(out, static_cast<std::int32_t>(values.size()));
    for (auto v : values) {
        write_var_int(out, v);
    }
}

void write_var_long_array(std::vector<std::uint8_t>& out, std::span<const std::int64_t> values) {
    write_var_int(out, static_cast<std::int32_t>(values.size()));
    for (auto v : values) {
        write_var_long(out, v);
    }
}

// ===== Fixed-Size Integer Decoders (Big-Endian) =====

std::int8_t read_byte(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 1);
    return static_cast<std::int8_t>(input[offset++]);
}

std::uint8_t read_ubyte(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 1);
    return input[offset++];
}

std::int16_t read_short(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 2);
    const auto hi = static_cast<std::int16_t>(input[offset]);
    const auto lo = static_cast<std::int16_t>(input[offset + 1]);
    offset += 2;
    return static_cast<std::int16_t>((hi << 8) | lo);
}

std::uint16_t read_ushort(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 2);
    const auto hi = static_cast<std::uint16_t>(input[offset]);
    const auto lo = static_cast<std::uint16_t>(input[offset + 1]);
    offset += 2;
    return static_cast<std::uint16_t>((hi << 8U) | lo);
}

std::int32_t read_int(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 4);
    std::int32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value = (value << 8) | static_cast<std::int32_t>(input[offset + i]);
    }
    offset += 4;
    return value;
}

std::uint32_t read_uint(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 4);
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value = (value << 8) | static_cast<std::uint32_t>(input[offset + i]);
    }
    offset += 4;
    return value;
}

std::int64_t read_long(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 8);
    std::int64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<std::int64_t>(input[offset + i]);
    }
    offset += 8;
    return value;
}

std::uint64_t read_ulong(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 8);
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | static_cast<std::uint64_t>(input[offset + i]);
    }
    offset += 8;
    return value;
}

// ===== Floating-Point Decoders (IEEE-754, Big-Endian) =====

float read_float(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto bits = read_uint(input, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(float));
    return value;
}

double read_double(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto bits = read_ulong(input, offset);
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(double));
    return value;
}

// ===== Byte Array Decoders =====

std::vector<std::uint8_t> read_byte_array(const std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto length = read_var_int(input, offset);
    if (length < 0) {
        throw std::runtime_error("Negative byte array length");
    }
    const auto safe_length = static_cast<std::size_t>(length);
    ensure_available(input, offset, safe_length);
    std::vector<std::uint8_t> value;
    value.insert(value.end(), input.begin() + static_cast<std::ptrdiff_t>(offset), input.begin() + static_cast<std::ptrdiff_t>(offset + safe_length));
    offset += safe_length;
    return value;
}

// ===== Angle & Rotation Decoders =====

float read_angle(std::span<const std::uint8_t> input, std::size_t& offset) {
    ensure_available(input, offset, 1);
    const auto byte_val = input[offset++];
    return static_cast<float>(byte_val) / 256.0f;
}

std::pair<float, float> read_rotation(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto yaw = read_angle(input, offset);
    const auto pitch = read_angle(input, offset);
    return {yaw, pitch};
}

// ===== Array Type Decoders =====

std::vector<std::int32_t> read_var_int_array(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto count = read_var_int(input, offset);
    if (count < 0) {
        throw std::runtime_error("Negative var_int array length");
    }
    std::vector<std::int32_t> values;
    values.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        values.push_back(read_var_int(input, offset));
    }
    return values;
}

std::vector<std::int64_t> read_var_long_array(std::span<const std::uint8_t> input, std::size_t& offset) {
    const auto count = read_var_int(input, offset);
    if (count < 0) {
        throw std::runtime_error("Negative var_long array length");
    }
    std::vector<std::int64_t> values;
    values.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        values.push_back(read_var_long(input, offset));
    }
    return values;
}

// ===== Compression Support (ZLib) =====

// Forward declaration
inline std::int32_t calculate_var_int_size(std::int32_t value);

std::vector<std::uint8_t> compress_packet(std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return {};
    }
    
    // Use zlib's compress2 for deflate compression
    std::vector<std::uint8_t> compressed(data.size() + 16); // Add some buffer
    uLongf compressed_size = compressed.size();
    
    const auto result = compress2(
        compressed.data(),
        &compressed_size,
        data.data(),
        data.size(),
        Z_DEFAULT_COMPRESSION
    );
    
    if (result != Z_OK) {
        throw std::runtime_error("Compression failed with zlib error: " + std::to_string(result));
    }
    
    compressed.resize(compressed_size);
    return compressed;
}

std::vector<std::uint8_t> decompress_packet(std::span<const std::uint8_t> compressed_data) {
    if (compressed_data.empty()) {
        return {};
    }
    
    // Start with an estimate for decompressed size
    std::vector<std::uint8_t> decompressed(compressed_data.size() * 4);
    
    while (true) {
        uLongf decompressed_size = decompressed.size();
        const auto result = uncompress(
            decompressed.data(),
            &decompressed_size,
            compressed_data.data(),
            compressed_data.size()
        );
        
        if (result == Z_OK) {
            decompressed.resize(decompressed_size);
            return decompressed;
        }
        
        if (result == Z_BUF_ERROR) {
            // Buffer too small, double it and try again
            decompressed.resize(decompressed.size() * 2);
            continue;
        }
        
        throw std::runtime_error("Decompression failed with zlib error: " + std::to_string(result));
    }
}

std::vector<std::uint8_t> encode_frame_compressed(
    const std::int32_t packet_id,
    const std::span<const std::uint8_t> payload,
    const std::int32_t compression_threshold) {
    
    if (compression_threshold < 0) {
        // No compression
        return encode_frame(packet_id, payload);
    }
    
    // Build the packet data (packet_id + payload)
    std::vector<std::uint8_t> packet_data;
    write_var_int(packet_data, packet_id);
    packet_data.insert(packet_data.end(), payload.begin(), payload.end());
    
    std::vector<std::uint8_t> frame;
    
    if (static_cast<std::int32_t>(packet_data.size()) >= compression_threshold) {
        // Compress the packet data
        const auto compressed = compress_packet(packet_data);
        
        // Frame format for compressed: frame_length (var_int) + data_length (var_int) + compressed_data
        // data_length is the length of the original packet data (non-zero indicates compressed)
        write_var_int(frame, static_cast<std::int32_t>(
            calculate_var_int_size(static_cast<std::int32_t>(packet_data.size())) + compressed.size()
        ));
        write_var_int(frame, static_cast<std::int32_t>(packet_data.size()));
        frame.insert(frame.end(), compressed.begin(), compressed.end());
    } else {
        // Too small to compress, send uncompressed
        // Frame format: frame_length (var_int) + data_length (var_int, = 0) + packet_data
        write_var_int(frame, static_cast<std::int32_t>(1 + packet_data.size())); // 1 byte for var_int 0
        write_var_int(frame, 0); // data_length = 0 means uncompressed
        frame.insert(frame.end(), packet_data.begin(), packet_data.end());
    }
    
    return frame;
}

bool try_decode_frame_compressed(
    const std::span<const std::uint8_t> input,
    std::size_t& consumed,
    const std::int32_t compression_threshold,
    EncodedFrame& frame) {
    
    if (compression_threshold < 0) {
        // No compression, use standard decoding
        return try_decode_frame(input, consumed, frame);
    }
    
    consumed = 0;
    std::int32_t frame_length = 0;
    std::size_t length_bytes = 0;
    
    if (!try_read_var_int(input, 0, frame_length, length_bytes)) {
        return false;
    }
    
    if (frame_length < 0) {
        throw std::runtime_error("Negative frame length");
    }
    
    if (input.size() < length_bytes + static_cast<std::size_t>(frame_length)) {
        return false;
    }
    
    const auto frame_data = input.subspan(length_bytes, static_cast<std::size_t>(frame_length));
    std::size_t offset = 0;
    
    const auto data_length = read_var_int(frame_data, offset);
    
    if (data_length == 0) {
        // Packet is not compressed
        const auto packet_data = frame_data.subspan(offset);
        std::size_t packet_offset = 0;
        frame.packet_id = read_var_int(packet_data, packet_offset);
        frame.payload.assign(packet_data.begin() + static_cast<std::ptrdiff_t>(packet_offset), packet_data.end());
    } else {
        // Packet is compressed
        const auto compressed_payload = frame_data.subspan(offset);
        const auto decompressed = decompress_packet(compressed_payload);
        std::size_t decomp_offset = 0;
        frame.packet_id = read_var_int(decompressed, decomp_offset);
        frame.payload.assign(decompressed.begin() + static_cast<std::ptrdiff_t>(decomp_offset), decompressed.end());
    }
    
    consumed = length_bytes + static_cast<std::size_t>(frame_length);
    return true;
}

// Helper: calculate the size of a var_int when encoded
inline std::int32_t calculate_var_int_size(std::int32_t value) {
    if ((value & 0xFFFFFF80) == 0) return 1;
    if ((value & 0xFFFFC000) == 0) return 2;
    if ((value & 0xFFE00000) == 0) return 3;
    if ((value & 0xF0000000) == 0) return 4;
    return 5;
}

} // namespace kprotocol::codec
