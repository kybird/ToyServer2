#pragma once
#include "Game/ObjectManager.h"
#include "Entity/Player.h"
#include "System/IObjectPool.h"
#include "System/Memory/SimplePool.h"

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

    std::shared_ptr<Player> CreatePlayer(int32_t gameId, System::ISession *session);
    void Release(Player *player);

private:
    PlayerFactory();

    System::SimplePool<Player> _pool;
};

} // namespace SimpleGame
