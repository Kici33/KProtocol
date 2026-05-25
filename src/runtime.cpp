#include "kprotocol/runtime.hpp"

#include "kprotocol/initialize.hpp"

namespace kprotocol {

ProtocolRuntime::ProtocolRuntime() {
    initialize(registry, translator);
}

} // namespace kprotocol
