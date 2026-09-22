#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace kprotocol {

// Thread-safe, bounded FIFO. A batch is accepted or rejected as a whole.
class MessageQueue {
public:
    explicit MessageQueue(std::size_t capacity = 100,
        std::chrono::milliseconds ttl = std::chrono::seconds(60));
    bool push(const std::vector<std::string>& messages);
    std::optional<std::string> pop();
    void clear();

private:
    using Clock = std::chrono::steady_clock;
    void expire(); // Caller holds mutex_.
    std::mutex mutex_;
    std::deque<std::pair<Clock::time_point, std::string>> messages_;
    std::size_t capacity_;
    std::chrono::milliseconds ttl_;
};

} // namespace kprotocol
