#include "kprotocol/protocol_server.hpp"

namespace kprotocol {

ProtocolServer::ProtocolServer()
    : server_(runtime_.registry, runtime_.translator) {}

} // namespace kprotocol
