#include "Room.h"

namespace SimpleGame {

Room::Room(int roomId) : _roomId(roomId)
{
}

void Room::Enter(std::shared_ptr<Player> player)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _players[player->GetSessionId()] = player;
    LOG_INFO("Player {} entered Room {}", player->GetSessionId(), _roomId);

    // Notify others? (TODO)
}

void Room::Leave(uint64_t sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _players.erase(sessionId);
    LOG_INFO("Player {} left Room {}", sessionId, _roomId);
}

void Room::Broadcast(System::PacketMessage *packet)
{
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto &pair : _players)
    {
        // Send to each session
        // Note: Packet refCount is managed. Send increments.
        pair.second->GetSession()->Send(packet);
    }
}

size_t Room::GetPlayerCount()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _players.size();
}

} // namespace SimpleGame
