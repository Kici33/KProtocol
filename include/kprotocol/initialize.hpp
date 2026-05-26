#pragma once

#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

namespace kprotocol {

// Register baseline packets, generated catalog (when compiled in), and all
// translation rules / ID mappings. Call once at startup.
void initialize(PacketRegistry& registry, PacketTranslator& translator);

} // namespace kprotocol
