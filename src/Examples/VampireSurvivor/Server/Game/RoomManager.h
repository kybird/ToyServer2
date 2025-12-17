#pragma once
#include "Room.h"
#include "../System/UserDB.h"
#include <map>

namespace SimpleGame {

class RoomManager
{
public:
    static RoomManager &Instance()
    {
        static RoomManager instance;
        return instance;
    }

    void Init(std::shared_ptr<System::ITimer> timer, std::shared_ptr<UserDB> userDB)
    {
        _timer = timer;
        _userDB = userDB;
        // Re-create default room with timer
        if (_rooms.empty())
        {
             CreateRoom(1);
        }
    }

    std::shared_ptr<Room> CreateRoom(int roomId)
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

    std::shared_ptr<Room> GetRoom(int roomId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _rooms.find(roomId);
        if (it != _rooms.end())
            return it->second;
        return nullptr;
    }

private:
    std::map<int, std::shared_ptr<Room>> _rooms;
    std::mutex _mutex;
    std::shared_ptr<System::ITimer> _timer;
    std::shared_ptr<UserDB> _userDB;
};

} // namespace SimpleGame
