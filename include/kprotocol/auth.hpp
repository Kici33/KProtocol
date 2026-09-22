#pragma once

#include <atomic>
#include <string>

namespace kprotocol {

struct MinecraftIdentity {
    std::string username;
    std::string uuid;
    std::string access_token;
};

// Authentication providers are used sequentially on the client's connection thread.
// Applications can implement another provider without changing MinecraftClient.
class MinecraftAuth {
public:
    virtual ~MinecraftAuth() = default;
    virtual MinecraftIdentity login(const std::atomic_bool& stop) = 0;
    virtual void join(const MinecraftIdentity& identity, const std::string& server_hash,
                      const std::atomic_bool& stop) = 0;
};

} // namespace kprotocol
