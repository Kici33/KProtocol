// Tests for the ProtocolVersion table introduced in Wave 1.
//
// Verifies:
//   - known wire numbers map to the canonical enumerator
//   - unknown wire numbers do NOT silently coerce into a known enumerator
//   - name_of() returns a stable, helpful name for both known and unknown wires
//   - protocol_number() round-trips through the enum

#include "kprotocol/version.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_known_wires_round_trip() {
    std::cout << "  Known wires round-trip through enum... ";

    // All names that are referenced by tests/examples must be present.
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_8) == 47);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_12_2) == 340);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_16_5) == 754);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_20_4) == 765);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_21_1) == 767);

    // Newly added versions are reachable.
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_19_4) == 762);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_21_5) == 770);

    std::cout << "ok\n";
}

void test_try_from_wire_known() {
    std::cout << "  try_from_wire on known wires... ";
    const auto v = kprotocol::try_from_wire(765);
    assert(v.has_value());
    assert(*v == kprotocol::ProtocolVersion::v1_20_4);
    assert(kprotocol::is_known_protocol(765));
    std::cout << "ok\n";
}

void test_try_from_wire_unknown() {
    std::cout << "  try_from_wire on unknown wires... ";
    // Pick a number that is NOT in the X-macro table.
    const auto v = kprotocol::try_from_wire(99999);
    assert(!v.has_value());
    assert(!kprotocol::is_known_protocol(99999));
    std::cout << "ok\n";
}

void test_name_of_known() {
    std::cout << "  name_of for known versions... ";
    assert(kprotocol::name_of(kprotocol::ProtocolVersion::v1_8) == std::string("1.8"));
    assert(kprotocol::name_of(kprotocol::ProtocolVersion::v1_20_4) == std::string("1.20.4"));
    assert(kprotocol::name_of(767) == std::string("1.21.1"));
    std::cout << "ok\n";
}

void test_name_of_unknown_is_stable() {
    std::cout << "  name_of for unknown wires returns protocol_<n>... ";
    assert(kprotocol::name_of(99999) == std::string("protocol_99999"));
    std::cout << "ok\n";
}

void test_enum_preserves_wire_for_unknown_cast() {
    std::cout << "  static_cast<ProtocolVersion>(unknown_wire) preserves wire number... ";
    // Even though the wire number is not in the X-macro, casting to enum and
    // back through protocol_number() preserves it. This is intentional: the
    // registry keys on the underlying int, so cross-version handshakes from
    // patch releases the library was not compiled with still work.
    const auto v = static_cast<kprotocol::ProtocolVersion>(12345);
    assert(kprotocol::protocol_number(v) == 12345);
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "version_tests:\n";
    test_known_wires_round_trip();
    test_try_from_wire_known();
    test_try_from_wire_unknown();
    test_name_of_known();
    test_name_of_unknown_is_stable();
    test_enum_preserves_wire_for_unknown_cast();
    std::cout << "All version-table tests passed.\n";
    return 0;
}
