#include "Game/RoomManager.h"
#include "Core/UserDB.h"
#include "System/IFramework.h"
#include "System/ISession.h"
#include "System/ITimer.h"
#include "System/Packet/IPacket.h"
#include "System/Packet/PacketBroadcast.h"
#include "System/Thread/IStrand.h"

static_assert(true, "RoomManager.cpp compiled");

namespace SimpleGame {

#pragma message("Compiling RoomManager::Init")
void RoomManager::Init(std::shared_ptr<System::IFramework> framework, std::shared_ptr<UserDB> userDB)
{
    _framework = framework;
    _timer = framework->GetTimer(); // Convenience
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

std::shared_ptr<Room> RoomManager::CreateRoom(int roomId, const std::string &title)
{
    std::cout << "[DEBUG] RoomManager::CreateRoom(" << roomId << ", " << title << ") Entry" << std::endl;
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_timer)
    {
        // ...
    }
    // Create Strand for this room
    std::shared_ptr<System::IStrand> strand = nullptr;
    if (_framework)
    {
        std::cout << "[DEBUG] Creating Strand..." << std::endl;
        strand = _framework->CreateStrand();
        std::cout << "[DEBUG] Strand Created." << std::endl;
    }

    std::cout << "[DEBUG] Creating Room Object..." << std::endl;
    std::shared_ptr<Room> newRoom = std::make_shared<Room>(roomId, _timer, strand, _userDB);
    newRoom->SetTitle(title);

    std::cout << "[DEBUG] Room Object Created. Adding to map..." << std::endl;
    _rooms[roomId] = newRoom;
    std::cout << "[DEBUG] Starting Room..." << std::endl;
    newRoom->Start(); // Auto start
    std::cout << "[DEBUG] Room Started. Returning." << std::endl;
    return newRoom;
}

void RoomManager::DestroyRoom(int roomId)
{
    // Don't destroy default room 1
    if (roomId == 1)
        return;

    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rooms.find(roomId);
    if (it != _rooms.end())
    {
        it->second->Stop();
        _rooms.erase(it);
        LOG_INFO("Room {} destroyed and removed from RoomManager.", roomId);
    }
}

std::shared_ptr<Room> RoomManager::GetRoom(int roomId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _rooms.find(roomId);
    if (it != _rooms.end())
        return it->second;
    return nullptr;
}

std::vector<std::shared_ptr<Room>> RoomManager::GetAllRooms()
{
    std::lock_guard<std::mutex> lock(_mutex);
    std::vector<std::shared_ptr<Room>> rooms;
    rooms.reserve(_rooms.size());

    for (auto &pair : _rooms)
    {
        rooms.push_back(pair.second);
    }

    return rooms;
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

void RoomManager::BroadcastPacketToLobby(const System::IPacket &pkt)
{
    LOG_INFO("Broadcasting packet to lobby");
    std::lock_guard<std::mutex> lock(_mutex);
    LOG_INFO("Lock is Problem");
    // Collect real sessions
    std::vector<System::Session *> realSessions;
    realSessions.reserve(_lobbySessions.size());

    for (auto &pair : _lobbySessions)
    {
        auto isess = pair.second;
        if (!isess)
            continue;

        auto sess = dynamic_cast<System::Session *>(isess);
        if (sess)
        {
            realSessions.push_back(sess);
        }
        else
        {
            isess->SendPacket(pkt);
        }
    }

    if (!realSessions.empty())
    {
        LOG_INFO("realSessions is not empty");
        System::PacketBroadcast::Broadcast(pkt, realSessions);
    }
    LOG_INFO("Broadcasting packet to lobby done");
}

} // namespace SimpleGame
