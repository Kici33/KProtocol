#pragma once

#include "kprotocol/auth.hpp"
#include "kprotocol/http.hpp"

#include <chrono>
#include <filesystem>
#include <functional>

namespace kprotocol {

struct MicrosoftDeviceCode {
    std::string verification_uri;
    std::string user_code;
    std::chrono::seconds expires_in{};
};

struct MicrosoftAuthOptions {
    std::string client_id;
    // Empty means memory-only authentication. Paths are used as supplied by the caller.
    std::filesystem::path cache_path;
    // Required when interactive sign-in is needed; called on the login thread.
    std::function<void(const MicrosoftDeviceCode&)> on_device_code;
    std::function<void(const std::string&)> on_log;
};

class MicrosoftAuth final : public MinecraftAuth {
public:
    explicit MicrosoftAuth(MicrosoftAuthOptions options, http::Transport transport = http::request);
    MinecraftIdentity login(const std::atomic_bool& stop) override;
    void join(const MinecraftIdentity& identity, const std::string& server_hash,
              const std::atomic_bool& stop) override;
    // Discard the in-memory Minecraft access token; next login refreshes it.
    void invalidate() noexcept;

private:
    MicrosoftAuthOptions options_;
    http::Transport transport_;
    MinecraftIdentity identity_;
    std::string refresh_token_;
    std::chrono::steady_clock::time_point expires_{};
    bool cache_loaded_{false};
    void log(const std::string& message) const;
};

} // namespace kprotocol
