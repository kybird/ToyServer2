#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Game/Room.h"
#include "System/Framework/Framework.h"
#include "System/Network/NetworkImpl.h"
#include "System/Network/WebSocketNetworkImpl.h"
#include "System/Utility/Json.h"

namespace SimpleGame {

void Room::BroadcastDebugState()
{
    // Throttle broadcast (e.g., 20 FPS = 0.05s)
    if (_totalRunTime - _debugBroadcastTimer < 0.05f)
        return;

    _debugBroadcastTimer = _totalRunTime;

    if (!_framework)
        return;

    // Safety check for Network casting
    auto network = std::dynamic_pointer_cast<System::NetworkImpl>(_framework->GetNetwork());
    if (!network)
        return;

    auto *ws = network->GetWebSocket();
    if (ws == nullptr)
        return;

    // Prepare JSON Data using System::Json
    System::Json root;
    root["rid"] = _roomId;
    root["t"] = _serverTick;

    // Players (이미 Strand에서 직렬화되어 있으므로 mutex 불필요)
    std::vector<System::Json> players;
    for (const auto &pair : _players)
    {
        auto p = pair.second;
        players.push_back({
            {"id", p->GetId()},
            {"x", p->GetX()},
            {"y", p->GetY()},
            {"hp", p->GetHp()},
            {"l", p->GetLookLeft()} // Direction: 1 for Left, 0 for Right
        });
    }
    root["p"] = players;

    // Monsters & Projectiles
    std::vector<System::Json> monsters;
    std::vector<System::Json> projectiles;
    const auto &objects = _objMgr.GetAllObjects();
    for (const auto &obj : objects)
    {
        if (obj->IsDead())
            continue;

        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            monsters.push_back({
                {"id", obj->GetId()}, {"x", obj->GetX()}, {"y", obj->GetY()}, {"t", 1} // Type ID placeholder
            });
        }
        else if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
        {
            projectiles.push_back({{"id", obj->GetId()}, {"x", obj->GetX()}, {"y", obj->GetY()}});
        }
    }
    root["m"] = monsters;
    root["pr"] = projectiles; // "pr" for projectiles

    // Broadcast
    ws->Broadcast(System::ToJsonString(root));
}

void Room::BroadcastDebugClear()
{
    if (!_framework)
        return;

    auto network = std::dynamic_pointer_cast<System::NetworkImpl>(_framework->GetNetwork());
    if (!network)
        return;

    auto *ws = network->GetWebSocket();
    if (ws == nullptr)
        return;

    System::Json root;
    root["rid"] = _roomId;
    root["reset"] = true;
    ws->Broadcast(System::ToJsonString(root));
}

} // namespace SimpleGame
