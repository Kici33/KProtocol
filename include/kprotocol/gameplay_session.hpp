#pragma once

#include "kprotocol/types.hpp"

#include <cstdint>
#include <string>

namespace kprotocol {

enum class GameMode : std::int32_t {
    survival = 0,
    creative = 1,
    adventure = 2,
    spectator = 3,
};

struct PlayerProfile {
    std::string username;
    std::string uuid;
    bool authenticated{false};
};

struct PlayerLocation {
    double x{};
    double y{};
    double z{};
    float yaw{};
    float pitch{};
    bool on_ground{false};
    bool known{false};
};

struct GameplaySession {
    PlayerProfile profile;
    std::int32_t entity_id{-1};
    GameMode game_mode{GameMode::survival};
    std::string dimension{"minecraft:overworld"};
    PlayerLocation location;
    bool login_started{false};
    bool login_complete{false};
    bool configuration_complete{false};
    bool joined_game{false};
};

} // namespace kprotocol
