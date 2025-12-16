#pragma once
#include "Room.h"
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

    RoomManager()
    {
        // Determine Default Room
        CreateRoom(1);
    }

    std::shared_ptr<Room> CreateRoom(int roomId)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto room = std::make_shared<Room>(roomId);
        _rooms[roomId] = room;
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
};

} // namespace SimpleGame
