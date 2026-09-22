#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace kprotocol {

// Returns false when cancelled, true after the requested delay. At most 50ms
// cancellation latency; no application logging or process-global stop state.
inline bool pause(const std::atomic_bool& stop, int milliseconds) {
    using namespace std::chrono;
    const auto end = steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!stop) {
        const auto remaining = end - steady_clock::now();
        if (remaining <= steady_clock::duration::zero()) return true;
        std::this_thread::sleep_for(std::min(remaining, duration_cast<steady_clock::duration>(50ms)));
    }
    return false;
}

} // namespace kprotocol
