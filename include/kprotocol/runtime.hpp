#pragma once

#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

namespace kprotocol {

// Owns an initialized registry + translator pair (baseline, generated catalog,
// and translation rules). Prefer this over manual initialize() boilerplate.
struct ProtocolRuntime {
    PacketRegistry registry;
    PacketTranslator translator;

    ProtocolRuntime();
};

} // namespace kprotocol
