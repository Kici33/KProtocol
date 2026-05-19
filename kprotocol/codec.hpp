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

void write_var_int(std::vector<std::uint8_t>& out, std::int32_t value);
void write_var_long(std::vector<std::uint8_t>& out, std::int64_t value);
void write_bool(std::vector<std::uint8_t>& out, bool value);
void write_u16(std::vector<std::uint8_t>& out, std::uint16_t value);
void write_string(std::vector<std::uint8_t>& out, const std::string& value);
void write_bytes(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value);

std::int32_t read_var_int(std::span<const std::uint8_t> input, std::size_t& offset);
std::int64_t read_var_long(std::span<const std::uint8_t> input, std::size_t& offset);
bool read_bool(std::span<const std::uint8_t> input, std::size_t& offset);
std::uint16_t read_u16(std::span<const std::uint8_t> input, std::size_t& offset);
std::string read_string(std::span<const std::uint8_t> input, std::size_t& offset);
std::vector<std::uint8_t> read_bytes(std::span<const std::uint8_t> input, std::size_t& offset);

std::vector<std::uint8_t> encode_frame(std::int32_t packet_id, std::span<const std::uint8_t> payload);
bool try_decode_frame(std::span<const std::uint8_t> input, std::size_t& consumed, EncodedFrame& frame);

} // namespace kprotocol::codec
