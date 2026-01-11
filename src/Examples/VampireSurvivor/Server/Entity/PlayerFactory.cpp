#include "PlayerFactory.h"
#include "System/Memory/SimplePool.h"
#include "Core/DataManager.h"

namespace SimpleGame {

PlayerFactory::PlayerFactory()
{
    _pool = std::make_unique<System::SimplePool<Player>>(100);
}

std::shared_ptr<Player> PlayerFactory::CreatePlayer(int32_t gameId, System::ISession *session)
{
    // ID is passed in (server-wide UserID or generated GameID?)
    // In Room.cpp, user ID is passed.

    Player *raw = _pool->Acquire();
    if (!raw)
        return nullptr;

    std::shared_ptr<Player> player(
        raw,
        [](Player *p)
        {
            PlayerFactory::Instance().Release(p);
        }
    );

    const auto *tmpl = DataManager::Instance().GetPlayerTemplate(1);
    int32_t hp = tmpl ? tmpl->hp : 100;
    float speed = tmpl ? tmpl->speed : 5.0f;

    player->Initialize(gameId, session, hp, speed);
    return player;
}

void PlayerFactory::Release(Player *player)
{
    player->Reset();
    _pool->Release(player);
}

} // namespace SimpleGame
