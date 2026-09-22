#pragma once

#include "kprotocol/auth.hpp"
#include "kprotocol/message_queue.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace kprotocol {

struct MinecraftClientOptions {
    std::string host = "localhost";
    std::uint16_t port = 25565;
    std::chrono::milliseconds send_interval{1500};
    std::size_t queue_capacity = 100;
    std::chrono::milliseconds message_ttl{60000};
    std::chrono::milliseconds reconnect_initial{5000};
    std::chrono::milliseconds reconnect_max{60000};
};

struct MinecraftChatMessage {
    std::string component_json;
    std::string text;
    std::uint8_t position{}; // 0 chat, 1 system, 2 action bar
};

struct MinecraftClientCallbacks {
    std::function<void(const MinecraftChatMessage&)> on_chat;
    std::function<void(const std::string& username)> on_ready;
    std::function<void(const std::string& reason)> on_disconnect;
    std::function<void(const std::string& message)> on_log;
};

// Stationary protocol-47 (Java 1.8.x) client. No guild, Discord or server-specific
// commands. Call connect/run on one thread; send_chat and ready are thread-safe.
// Callbacks execute on the connection thread and must not throw. Keep the object
// and auth provider alive until connect/run returns after cancellation.
class MinecraftClient {
public:
    using Join = std::function<void(const MinecraftIdentity&, const std::string&,
                                    const std::atomic_bool&)>;
    explicit MinecraftClient(MinecraftClientOptions options,
                             MinecraftClientCallbacks callbacks = {});
    // One connection; throws on errors/disconnect. An online-mode challenge
    // requires a join callback. Offline-mode servers do not invoke it.
    void connect(const MinecraftIdentity& identity, const std::atomic_bool& stop,
                 Join join = {});
    // Authenticate and reconnect with bounded backoff until cancelled.
    void run(MinecraftAuth& auth, const std::atomic_bool& stop);
    bool ready() const noexcept { return ready_; }
    // Reject while offline or full, or if any string is empty, contains invalid
    // UTF-8/control/formatting, or exceeds 100 bytes. No implicit chat command.
    bool send_chat(const std::string& message);
    bool send_chat(const std::vector<std::string>& messages);

private:
    void set_ready(bool ready);
    void log(const std::string& message) const;
    MinecraftClientOptions options_;
    MinecraftClientCallbacks callbacks_;
    MessageQueue outbound_;
    std::atomic_bool ready_{false};
    std::mutex state_mutex_;
    std::atomic_flag connecting_ = ATOMIC_FLAG_INIT;
};

} // namespace kprotocol
