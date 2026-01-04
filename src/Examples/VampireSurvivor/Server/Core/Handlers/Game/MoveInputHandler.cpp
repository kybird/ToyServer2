#include "MoveInputHandler.h"
#include "Protocol/game.pb.h"
#include "GamePackets.h"
#include "Game/RoomManager.h"
#include "Game/GameConfig.h"
#include "System/ILog.h"
#include "System/Thread/IStrand.h"

namespace SimpleGame {
namespace Handlers {
namespace Game {

void MoveInputHandler::Handle(System::ISession* session, System::PacketView packet)
{
    Protocol::C_MoveInput req;
    if (packet.Parse(req))
    {
        // Find Player using Global Map
        auto player = RoomManager::Instance().GetPlayer(session->GetId());
        if (player)
        {
            // [Strand] Use Room's Strand for processing
            auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
            if (room && room->GetStrand())
            {
                // Capture by value for thread safety
                uint32_t clientTick = req.client_tick();
                int32_t dx = req.dir_x();
                int32_t dy = req.dir_y();

                room->GetStrand()->Post(
                    [player, clientTick, dx, dy, room]()
                    {
                        // 1. Get current server tick
                        uint32_t serverTick = room->GetServerTick();

                        // 2. Calculate tick difference (server is ahead of client)
                        int32_t tickDiff = (int32_t)serverTick - (int32_t)clientTick;
                        if (tickDiff < 0)
                            tickDiff = 0; // Client sent future tick

                        // 3. Set input (updates velocity)
                        player->SetInput(clientTick, dx, dy);

                        // 4. clientTick 시점의 위치 계산 (입력 적용 후 1틱 이동)
                        float tickInterval = GameConfig::TICK_INTERVAL_SEC;
                        float deltaTime = 1 * tickInterval; // clientTick 시점의 1틱만 이동

                        float predictedX = player->GetX() + player->GetVX() * deltaTime;
                        float predictedY = player->GetY() + player->GetVY() * deltaTime;

                        // 5. Send immediate Ack with predicted position
                        Protocol::S_PlayerStateAck ack;
                        ack.set_server_tick(serverTick);
                        ack.set_client_tick(clientTick);
                        ack.set_x(predictedX);
                        ack.set_y(predictedY);

                        S_PlayerStateAckPacket ackPkt(ack);
                        if (player->GetSession())
                        {
                            LOG_INFO("ServerPos PlayerId={} Pos=({:.2f},{:.2f})", player->GetId(), player->GetX(), player->GetY());
                            player->GetSession()->SendPacket(ackPkt);

                            LOG_INFO(
                                "S_PlayerStateAck: ClientTick={}, ServerTick={}, Diff={}, Pos=({:.2f}, {:.2f})",
                                clientTick,
                                serverTick,
                                tickDiff,
                                predictedX,
                                predictedY
                            );
                        }
                    }
                );
            }
            else
            {
                LOG_WARN("Room/Strand not found for C_MOVE_INPUT");
            }
        }
        else
        {
            LOG_WARN("C_MOVE_INPUT from unknown session {}", session->GetId());
        }
    }
}

} // namespace Game
} // namespace Handlers
} // namespace SimpleGame
