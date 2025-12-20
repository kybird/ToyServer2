#pragma once
#include "Game/Room.h"
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace System {
class ITimer;
class ISession;
class IPacket;
class IFramework;
} // namespace System

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

    void Init(std::shared_ptr<System::IFramework> framework, std::shared_ptr<UserDB> userDB);
    void TestMethod(); // Test linking

    std::shared_ptr<Room> CreateRoom(int roomId);
    void DestroyRoom(int roomId);
    std::shared_ptr<Room> GetRoom(int roomId);

    void RegisterPlayer(uint64_t sessionId, std::shared_ptr<Player> player);
    void UnregisterPlayer(uint64_t sessionId);
    std::shared_ptr<Player> GetPlayer(uint64_t sessionId);

    // Lobby Management
    void EnterLobby(System::ISession *session);
    void LeaveLobby(uint64_t sessionId);
    bool IsInLobby(uint64_t sessionId);
    void BroadcastPacketToLobby(const System::IPacket &pkt);

private:
    std::map<int, std::shared_ptr<Room>> _rooms;
    std::map<uint64_t, std::shared_ptr<Player>> _players;
    std::map<uint64_t, System::ISession *> _lobbySessions; // Session ID -> Session Pointer
    std::mutex _mutex;
    std::shared_ptr<System::IFramework> _framework;
    std::shared_ptr<System::ITimer> _timer;
    std::shared_ptr<UserDB> _userDB;
};

} // namespace SimpleGame
