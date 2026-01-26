#include "CreateRoomHandler.h"
#include "Game/RoomManager.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/ILog.h"
#include <iostream>
#include <string>

namespace SimpleGame {
namespace Handlers {
namespace Room {

void CreateRoomHandler::Handle(System::SessionContext &ctx, System::PacketView packet)
{
    std::cout << "[DEBUG] Handling C_CREATE_ROOM" << std::endl;
    Protocol::C_CreateRoom req;
    if (packet.Parse(req))
    {
        std::cout << "[DEBUG] Parsed C_CREATE_ROOM" << std::endl;

        static int nextRoomId = 2; // Simple auto-increment
        int newRoomId = nextRoomId++;

        std::string title = req.room_title();
        if (title.empty())
            title = "Room " + std::to_string(newRoomId);

        auto room = RoomManager::Instance().CreateRoom(newRoomId, title);

        Protocol::S_CreateRoom res;
        res.set_success(true);
        res.set_room_id(newRoomId);

        S_CreateRoomPacket respPacket(res);
        ctx.Send(respPacket);
        LOG_INFO("Created Room {}", newRoomId);
    }
}

} // namespace Room
} // namespace Handlers
} // namespace SimpleGame
