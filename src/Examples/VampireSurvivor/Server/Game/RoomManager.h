#pragma once
#include "Game/Room.h"
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>

namespace System {
class ITimer;
}

namespace SimpleGame {
class UserDB;
class Player;

class RoomManager
{
public:
    static RoomManager &Instance()
    {
        static RoomManager instance;
        return instance;
    }

    void Init(std::shared_ptr<System::ITimer> timer, std::shared_ptr<UserDB> userDB);
    void TestMethod(); // Test linking

    std::shared_ptr<Room> CreateRoom(int roomId);
    std::shared_ptr<Room> GetRoom(int roomId);

    void RegisterPlayer(uint64_t sessionId, std::shared_ptr<Player> player);
    void UnregisterPlayer(uint64_t sessionId);
    std::shared_ptr<Player> GetPlayer(uint64_t sessionId);

private:
    std::map<int, std::shared_ptr<Room>> _rooms;
    std::map<uint64_t, std::shared_ptr<Player>> _players;
    std::mutex _mutex;
    std::shared_ptr<System::ITimer> _timer;
    std::shared_ptr<UserDB> _userDB;
};

} // namespace SimpleGame
