#pragma once
#include "../System/ObjectManager.h"
#include "Player.h"
#include "System/IObjectPool.h"
#include "System/Memory/SimplePool.h"

namespace SimpleGame {

/**
 * @brief Factory for creating Players with pooling.
 */
class PlayerFactory {
public:
    static PlayerFactory& Instance() {
        static PlayerFactory instance;
        return instance;
    }

    std::shared_ptr<Player> CreatePlayer(ObjectManager& objMgr, int32_t gameId, System::ISession* session) {
        // ID is passed in (server-wide UserID or generated GameID?)
        // In Room.cpp, user ID is passed.
        
        Player* raw = _pool.Acquire();
        if (!raw) return nullptr;

        std::shared_ptr<Player> player(raw, [](Player* p) {
            PlayerFactory::Instance().Release(p);
        });

        player->Initialize(gameId, session);
        return player;
    }

    void Release(Player* player) {
        player->Reset();
        _pool.Release(player);
    }

private:
    PlayerFactory() : _pool(100) {} // Limit 100 players per room context? Or global? Singleton is global.

    System::SimplePool<Player> _pool;
};

} // namespace SimpleGame
