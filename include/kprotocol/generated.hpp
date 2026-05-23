#pragma once

#include "kprotocol/registry.hpp"

// Defined by CMake when the committed generated/ catalog is compiled into
// kprotocol_packets (KPROTOCOL_BUILD_GENERATED_PACKETS=ON and
// generated/registry/register_all_packets.cpp exists). Call sites should
// guard registration with #ifdef KPROTOCOL_GENERATED.
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#define KPROTOCOL_GENERATED 1

namespace kprotocol {

void register_generated_packets(PacketRegistry& registry);

} // namespace kprotocol

#endif
