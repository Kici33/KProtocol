#pragma once

#include "kprotocol/runtime.hpp"
#include "kprotocol/server.hpp"

namespace kprotocol {

// Ready-to-run server: initialized registry/translator plus TCP runtime.
class ProtocolServer {
public:
    ProtocolServer();

    [[nodiscard]] PacketRegistry& registry() noexcept { return runtime_.registry; }
    [[nodiscard]] PacketTranslator& translator() noexcept { return runtime_.translator; }
    [[nodiscard]] MinecraftServer& server() noexcept { return server_; }

    [[nodiscard]] const PacketRegistry& registry() const noexcept { return runtime_.registry; }
    [[nodiscard]] const PacketTranslator& translator() const noexcept { return runtime_.translator; }
    [[nodiscard]] const MinecraftServer& server() const noexcept { return server_; }

private:
    ProtocolRuntime runtime_;
    MinecraftServer server_;
};

} // namespace kprotocol
