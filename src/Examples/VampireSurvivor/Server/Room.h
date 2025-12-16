#pragma once
#include "Player.h"
#include "Protocol.h"
#include "System/ILog.h"
#include "System/Network/PacketUtils.h"
#include <mutex>
#include <unordered_map>
#include <vector>


namespace SimpleGame {

class Room
{
public:
    Room(int roomId);

    void Enter(std::shared_ptr<Player> player);
    void Leave(uint64_t sessionId);
    void Broadcast(System::PacketMessage *packet);

    int GetId() const
    {
        return _roomId;
    }
    size_t GetPlayerCount();

private:
    int _roomId;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;
    std::mutex _mutex;
};

} // namespace SimpleGame
