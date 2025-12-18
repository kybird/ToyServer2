#include "PlayerFactory.h"
#include "System/Memory/SimplePool.h"

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

    player->Initialize(gameId, session);
    return player;
}

void PlayerFactory::Release(Player *player)
{
    player->Reset();
    _pool->Release(player);
}

} // namespace SimpleGame
