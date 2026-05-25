#include "kprotocol/version.hpp"

#include <string>
#include <unordered_map>

namespace kprotocol {

namespace {

// Built once at static-init time from the X-macro. Looking up an int32 is O(1).
const std::unordered_map<std::int32_t, std::string_view>& wire_to_name() {
    static const std::unordered_map<std::int32_t, std::string_view> map = [] {
        std::unordered_map<std::int32_t, std::string_view> m;
#define KPROTOCOL_X(name, wire, display) m.emplace(wire, display);
        KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
        return m;
    }();
    return map;
}

} // namespace

std::optional<ProtocolVersion> try_from_wire(std::int32_t wire) noexcept {
    const auto& m = wire_to_name();
    if (m.find(wire) == m.end()) {
        return std::nullopt;
    }
    return static_cast<ProtocolVersion>(wire);
}

bool is_known_protocol(std::int32_t wire) noexcept {
    const auto& m = wire_to_name();
    return m.find(wire) != m.end();
}

std::string name_of(std::int32_t wire) {
    const auto& m = wire_to_name();
    if (const auto it = m.find(wire); it != m.end()) {
        return std::string(it->second);
    }
    return "protocol_" + std::to_string(wire);
}

std::string name_of(ProtocolVersion version) {
    return name_of(protocol_number(version));
}

ProtocolVersion catalog_anchor_for(const std::int32_t wire) noexcept {
    std::int32_t anchor_wire = protocol_number(ProtocolVersion::v1_8);
    if (wire < anchor_wire) {
        return static_cast<ProtocolVersion>(wire);
    }
#define KPROTOCOL_X(name, known_wire, display) \
    if (known_wire <= wire && known_wire >= anchor_wire) { \
        anchor_wire = known_wire; \
    }
    KPROTOCOL_FOR_EACH_KNOWN_VERSION(KPROTOCOL_X)
#undef KPROTOCOL_X
    return static_cast<ProtocolVersion>(anchor_wire);
}

} // namespace kprotocol
