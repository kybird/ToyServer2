#include "Game/RoomManager.h"
#include "System/ITimer.h"
#include "Core/UserDB.h"

static_assert(true, "RoomManager.cpp compiled");

namespace SimpleGame {

    #pragma message("Compiling RoomManager::Init")
void RoomManager::Init(std::shared_ptr<System::ITimer> timer, std::shared_ptr<UserDB> userDB)
{
    _timer = timer;
    _userDB = userDB;
    // Re-create default room with timer
    if (_rooms.empty())
    {
        CreateRoom(1);
    }
}

void RoomManager::TestMethod() {
    // Empty test
}

std::shared_ptr<Room> RoomManager::CreateRoom(int roomId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_timer)
    {
        // If Init not called yet, wait or creates without timer?
        // Better to warn. For now, just create.
    }
    auto room = std::make_shared<Room>(roomId, _timer, _userDB);
    _rooms[roomId] = room;
    room->Start(); // Auto start
    return room;
}

std::shared_ptr<Room> RoomManager::GetRoom(int roomId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rooms.find(roomId);
    if (it != _rooms.end())
        return it->second;
    return nullptr;
}

void RoomManager::RegisterPlayer(uint64_t sessionId, std::shared_ptr<Player> player)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _players[sessionId] = player;
}

void RoomManager::UnregisterPlayer(uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _players.erase(sessionId);
}

std::shared_ptr<Player> RoomManager::GetPlayer(uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _players.find(sessionId);
    if (it != _players.end())
        return it->second;
    return nullptr;
}

} // namespace SimpleGame
