#pragma once

#include <cstdint>

namespace kprotocol::codec::limits {

// Maximum length of a single packet frame (Minecraft's vanilla cap is ~2 MiB).
// Frames whose declared length exceeds this MUST be rejected by the decoder.
inline constexpr std::int32_t max_packet_length = 2 * 1024 * 1024;

// Maximum length of an encoded Java string (mojang's CHAT_STRING/IDENTIFIER caps).
// 32767 = Short.MAX_VALUE, the conservative ceiling used across all versions.
inline constexpr std::int32_t max_string_length = 32767;

// Maximum length of the decompressed payload returned by decompress_packet().
// Bounds the worst-case allocation a malicious peer can force.
inline constexpr std::int32_t max_decompressed_length = 8 * 1024 * 1024;

// Wire-format ceiling on VarInt / VarLong continuation length.
inline constexpr std::size_t max_var_int_bytes  = 5;
inline constexpr std::size_t max_var_long_bytes = 10;

} // namespace kprotocol::codec::limits
