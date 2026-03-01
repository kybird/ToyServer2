#pragma once
#include "Entity/Player.h"
#include "Game/ObjectManager.h"
#include "System/Memory/LockFreeObjectPool.h"
#include "System/Memory/RefPtr.h"

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

    ::System::RefPtr<Player> CreatePlayer(int32_t gameId, uint64_t sessionId);
    void Release(Player *player);

private:
    PlayerFactory();

    std::unique_ptr<::System::LockFreeObjectPool<Player>> _pool;
};

} // namespace SimpleGame
