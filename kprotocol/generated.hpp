#pragma once

#include "kprotocol/registry.hpp"

#define KPROTOCOL_GENERATED 1

namespace kprotocol {

// Generated packet registration entrypoint. Implemented in generated/registry/register_all_packets.cpp
void register_generated_packets(PacketRegistry& registry);

} // namespace kprotocol
