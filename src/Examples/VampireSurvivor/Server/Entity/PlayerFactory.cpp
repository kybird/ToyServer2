#include "PlayerFactory.h"
#include "Core/DataManager.h"
#include "Game/GameConfig.h"

namespace SimpleGame {

PlayerFactory::PlayerFactory()
{
    _pool = std::make_unique<::System::LockFreeObjectPool<Player>>();
    _pool->Init(0, 1000);
}

::System::RefPtr<Player> PlayerFactory::CreatePlayer(int32_t gameId, uint64_t sessionId)
{
    ::System::RefPtr<Player> player = _pool->Pop();
    if (!player)
        return nullptr;

    const auto *tmpl = DataManager::Instance().GetPlayerInfo(1);
    int32_t hp = tmpl ? tmpl->hp : GameConfig::DEFAULT_PLAYER_HP;
    float speed = tmpl ? tmpl->speed : GameConfig::DEFAULT_PLAYER_SPEED;

    player->Initialize(gameId, sessionId, hp, speed);
    return player;
}

void PlayerFactory::Release(Player *player)
{
    // _pool->Push(player) will be called internally when RefCount hits 0.
}

} // namespace SimpleGame
