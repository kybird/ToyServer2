#pragma once
#include "Entity/Player.h"
#include "Game/ObjectManager.h"
#include "System/IObjectPool.h"

namespace SimpleGame {

/**
 * @brief Factory for creating Players with pooling.
 */
class PlayerFactory
{
public:
    static PlayerFactory &Instance()
    {
        static PlayerFactory instance;
        return instance;
    }

    std::shared_ptr<Player> CreatePlayer(int32_t gameId, uint64_t sessionId);
    void Release(Player *player);

private:
    PlayerFactory();

    std::unique_ptr<System::IObjectPool<Player>> _pool;
};

} // namespace SimpleGame
