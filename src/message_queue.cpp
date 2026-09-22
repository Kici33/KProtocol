#include "kprotocol/message_queue.hpp"
#include <stdexcept>

namespace kprotocol {

MessageQueue::MessageQueue(std::size_t capacity, std::chrono::milliseconds ttl)
    : capacity_(capacity), ttl_(ttl) {
    if (!capacity || ttl.count() <= 0) throw std::invalid_argument("Message queue requires positive capacity and TTL");
}
void MessageQueue::expire() {
    const auto now = Clock::now();
    while (!messages_.empty() && now - messages_.front().first >= ttl_) messages_.pop_front();
}
bool MessageQueue::push(const std::vector<std::string>& messages) {
    std::lock_guard lock(mutex_);
    expire();
    if (messages.size() > capacity_ - messages_.size()) return false;
    const auto now = Clock::now();
    for (const auto& message : messages) messages_.emplace_back(now, message);
    return true;
}
std::optional<std::string> MessageQueue::pop() {
    std::lock_guard lock(mutex_);
    expire();
    if (messages_.empty()) return std::nullopt;
    auto message = std::move(messages_.front().second);
    messages_.pop_front();
    return message;
}
void MessageQueue::clear() { std::lock_guard lock(mutex_); messages_.clear(); }

} // namespace kprotocol
