// Tests for the ProtocolVersion table introduced in Wave 1.
//
// Verifies:
//   - known wire numbers map to the canonical enumerator
//   - unknown wire numbers do NOT silently coerce into a known enumerator
//   - name_of() returns a stable, helpful name for both known and unknown wires
//   - protocol_number() round-trips through the enum
//   - WireProtocol preserves arbitrary client wire numbers

#include "kprotocol/version.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

void test_known_wires_round_trip() {
    std::cout << "  Known wires round-trip through enum... ";

    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_8) == 47);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_12_2) == 340);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_16_5) == 754);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_20_4) == 765);
    assert(kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_21_1) == 767);

    assert(kprotocol::protocol_number(kprotocol::KnownVersion::v1_19_4) == 762);
    assert(kprotocol::protocol_number(kprotocol::KnownVersion::v1_21_5) == 770);
    assert(kprotocol::protocol_number(kprotocol::KnownVersion::v1_21_11) == 774);
    assert(kprotocol::protocol_number(kprotocol::latest_catalog_version()) == 774);
    assert(kprotocol::wire_number(kprotocol::latest_known_version()) == 774);

    std::cout << "ok\n";
}

void test_try_from_wire_known() {
    std::cout << "  try_from_wire on known wires... ";
    const auto v = kprotocol::try_from_wire(765);
    assert(v.has_value());
    assert(*v == kprotocol::ProtocolVersion::v1_20_4);
    assert(kprotocol::is_known_protocol(765));
    const auto known = kprotocol::try_from_wire(kprotocol::WireProtocol{765});
    assert(known.has_value());
    assert(*known == kprotocol::KnownVersion::v1_20_4);
    std::cout << "ok\n";
}

void test_try_from_wire_unknown() {
    std::cout << "  try_from_wire on unknown wires... ";
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
    assert(kprotocol::name_of(kprotocol::WireProtocol{767}) == std::string("1.21.1"));
    std::cout << "ok\n";
}

void test_name_of_unknown_is_stable() {
    std::cout << "  name_of for unknown wires returns protocol_<n>... ";
    assert(kprotocol::name_of(99999) == std::string("protocol_99999"));
    std::cout << "ok\n";
}

void test_wire_protocol_preserves_unknown() {
    std::cout << "  WireProtocol stores arbitrary client wire numbers... ";
    const kprotocol::WireProtocol wire{12345};
    assert(wire.value == 12345);
    assert(kprotocol::catalog_anchor_for(wire) == kprotocol::ProtocolVersion::v1_21_11);
    std::cout << "ok\n";
}

void test_catalog_anchor_for() {
    std::cout << "  catalog_anchor_for picks nearest compiled anchor... ";
    assert(kprotocol::catalog_anchor_for(47) == kprotocol::ProtocolVersion::v1_8);
    assert(kprotocol::catalog_anchor_for(754) == kprotocol::ProtocolVersion::v1_16_5);
    assert(kprotocol::catalog_anchor_for(775) == kprotocol::ProtocolVersion::v1_21_11);
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
    test_wire_protocol_preserves_unknown();
    test_catalog_anchor_for();
    std::cout << "All version-table tests passed.\n";
    return 0;
}
