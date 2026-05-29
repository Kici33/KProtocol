#include "kprotocol/configuration.hpp"

namespace kprotocol {

bool send_configuration_finish(const ClientSession& client) {
    if (!client.valid()) {
        return false;
    }
    return client.send_packet_direct(
        SFinishConfigurationPacket{}.to_packet(),
        client.protocol_version());
}

bool send_feature_flags(const ClientSession& client, const SFeatureFlagsPacket& packet) {
    if (!client.valid()) {
        return false;
    }
    return client.send_packet_direct(packet.to_packet(), client.protocol_version());
}

bool send_registry_data(const ClientSession& client, const SRegistryDataPacket& packet) {
    if (!client.valid()) {
        return false;
    }
    return client.send_packet_direct(packet.to_packet(), client.protocol_version());
}

} // namespace kprotocol
