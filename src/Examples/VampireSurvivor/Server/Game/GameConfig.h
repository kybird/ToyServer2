#pragma once

namespace SimpleGame {

struct GameConfig
{
    static constexpr int TPS = 25;
    static constexpr int TICK_INTERVAL_MS = 1000 / TPS;
    static constexpr float TICK_INTERVAL_SEC = 1.0f / static_cast<float>(TPS);
};

} // namespace SimpleGame
