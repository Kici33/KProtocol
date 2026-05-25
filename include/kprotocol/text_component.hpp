#pragma once

#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#include <string>

namespace kprotocol {

// Escape plain text into a minimal JSON text component string.
[[nodiscard]] std::string json_text(const std::string& plain);

// Build a root NBT text component `{text: "..."}` for 1.20.4+ UI packets.
[[nodiscard]] NBTBlob text_component_nbt(const std::string& plain);

// True when UI text fields use optional_nbt instead of JSON strings (765+).
[[nodiscard]] bool uses_nbt_text(ProtocolVersion version) noexcept;

} // namespace kprotocol
