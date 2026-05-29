#pragma once

#include "kprotocol/packets/configuration/SFeatureFlagsPacket.hpp"
#include "kprotocol/packets/configuration/SFinishConfigurationPacket.hpp"
#include "kprotocol/packets/configuration/SRegistryDataPacket.hpp"
#include "kprotocol/server.hpp"

namespace kprotocol {

bool send_configuration_finish(const ClientSession& client);
bool send_feature_flags(const ClientSession& client, const SFeatureFlagsPacket& packet = {});
bool send_registry_data(const ClientSession& client, const SRegistryDataPacket& packet);

} // namespace kprotocol
