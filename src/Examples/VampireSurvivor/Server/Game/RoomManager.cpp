#include "Game/RoomManager.h"
#include "Core/UserDB.h"
#include "System/ISession.h"
#include "System/ITimer.h"


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

void RoomManager::TestMethod()
{
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

void RoomManager::EnterLobby(System::ISession *session)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _lobbySessions[session->GetId()] = session;
}

void RoomManager::LeaveLobby(uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _lobbySessions.erase(sessionId);
}

bool RoomManager::IsInLobby(uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _lobbySessions.find(sessionId) != _lobbySessions.end();
}

void RoomManager::BroadcastToLobby(System::PacketMessage *packet)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto it = _lobbySessions.begin(); it != _lobbySessions.end();)
    {
        auto session = it->second;
        // Check if session is valid? ISession* doesn't guarantee validity if we don't own it.
        // Assuming session lifetime is managed outside and we are notified on close.
        // If session is closed, Send might fail or be safe?
        // Ideally we check implicit validity.
        // For now, simple iteration.
        session->Send(packet);
        ++it;
    }
}

} // namespace SimpleGame
