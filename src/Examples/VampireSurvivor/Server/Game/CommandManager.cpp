#include "CommandManager.h"
#include "Core/DataManager.h"
#include "Entity/Player.h"
#include "Entity/PlayerInventory.h"
#include "Game/Room.h"
#include "Game/RoomManager.h"
#include "System/ILog.h"
#include <algorithm>
#include <sstream>


namespace SimpleGame {

void CommandManager::Init()
{
    // 1. /levelup - 경험치 추가
    RegisterCommand(
        "/levelup",
        "Add EXP to players (Usage: /levelup [amount])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            int exp = 100;
            if (!args.empty())
            {
                try
                {
                    exp = std::stoi(args[0]);
                } catch (...)
                {
                }
            }

            if (ctx.FromServer())
            {
                auto rooms = RoomManager::Instance().GetAllRooms();
                for (auto &r : rooms)
                    r->DebugAddExpToAll(exp);
                LOG_INFO("[Command] Added {} EXP to all players.", exp);
            }
            else
            {
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                    player->AddExp(exp, room.get());
                    LOG_INFO("[Command] Added {} EXP to player {}.", exp, ctx.sessionId);
                }
            }
        }
    );

    // 2. /spawn - 몬스터 스폰
    RegisterCommand(
        "/spawn",
        "Spawn monsters (Usage: /spawn [id] [count])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.size() < 2)
                return;
            try
            {
                int id = std::stoi(args[0]);
                int count = std::stoi(args[1]);
                auto room = RoomManager::Instance().GetRoom(ctx.roomId);
                if (room)
                {
                    room->DebugSpawnMonster(id, count);
                    LOG_INFO("[Command] Spawned {} monsters (ID: {}) in room {}.", count, id, ctx.roomId);
                }
            } catch (...)
            {
            }
        }
    );

    // 3. /god - 무적 모드 토글
    RegisterCommand(
        "/god",
        "Toggle God Mode",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (ctx.FromServer())
            {
                auto rooms = RoomManager::Instance().GetAllRooms();
                for (auto &r : rooms)
                    r->DebugToggleGodMode();
                LOG_INFO("[Command] Toggled God Mode for all players.");
            }
            else
            {
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    player->SetGodMode(!player->IsGodMode());
                    LOG_INFO("[Command] Toggled God Mode for player {}: {}", ctx.sessionId, player->IsGodMode());
                }
            }
        }
    );

    // 4. /add_weapon - 무기 추가/업그레이드
    RegisterCommand(
        "/add_weapon",
        "Add or upgrade weapon (Usage: /add_weapon [id])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.empty() || ctx.FromServer())
                return;
            try
            {
                int id = std::stoi(args[0]);
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    if (player->GetInventory().AddOrUpgradeWeapon(id))
                    {
                        auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                        if (room)
                        {
                            player->RefreshInventoryEffects(room.get());
                            player->SyncInventory(room.get());
                        }
                        LOG_INFO("[Command] Player {} added weapon {}.", ctx.sessionId, id);
                    }
                }
            } catch (...)
            {
            }
        }
    );

    // 5. /remove_weapon - 무기 삭제
    RegisterCommand(
        "/remove_weapon",
        "Remove weapon (Usage: /remove_weapon [id])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.empty() || ctx.FromServer())
                return;
            try
            {
                int id = std::stoi(args[0]);
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    player->GetInventory().RemoveWeapon(id);
                    auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                    if (room)
                    {
                        player->RefreshInventoryEffects(room.get());
                        player->SyncInventory(room.get());
                    }
                    LOG_INFO("[Command] Player {} removed weapon {}.", ctx.sessionId, id);
                }
            } catch (...)
            {
            }
        }
    );

    // 6. /add_passive - 패시브 추가/업그레이드
    RegisterCommand(
        "/add_passive",
        "Add or upgrade passive (Usage: /add_passive [id])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.empty() || ctx.FromServer())
                return;
            try
            {
                int id = std::stoi(args[0]);
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    if (player->GetInventory().AddOrUpgradePassive(id))
                    {
                        auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                        if (room)
                        {
                            player->RefreshInventoryEffects(room.get());
                            player->SyncInventory(room.get());
                        }
                        LOG_INFO("[Command] Player {} added passive {}.", ctx.sessionId, id);
                    }
                }
            } catch (...)
            {
            }
        }
    );

    // 7. /remove_passive - 패시브 삭제
    RegisterCommand(
        "/remove_passive",
        "Remove passive (Usage: /remove_passive [id])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.empty() || ctx.FromServer())
                return;
            try
            {
                int id = std::stoi(args[0]);
                auto player = RoomManager::Instance().GetPlayer(ctx.sessionId);
                if (player)
                {
                    player->GetInventory().RemovePassive(id);
                    auto room = RoomManager::Instance().GetRoom(player->GetRoomId());
                    if (room)
                    {
                        player->RefreshInventoryEffects(room.get());
                        player->SyncInventory(room.get());
                    }
                    LOG_INFO("[Command] Player {} removed passive {}.", ctx.sessionId, id);
                }
            } catch (...)
            {
            }
        }
    );

    // 8. /ai - 몬스터 AI 변경
    RegisterCommand(
        "/ai",
        "Change AI strategy (Usage: /ai [strict|smart|fluid|surround|vampire])",
        [](const CommandContext &ctx, const std::vector<std::string> &args)
        {
            if (args.empty())
                return;
            std::string strategy = args[0];
            auto room = RoomManager::Instance().GetRoom(ctx.roomId);
            if (room)
            {
                room->SetMonsterStrategy(strategy);
                LOG_INFO("[Command] Changed AI strategy to {} in room {}.", strategy, ctx.roomId);
            }
        }
    );
}

void CommandManager::RegisterCommand(const std::string &command, const std::string &description, CommandHandler handler)
{
    _commands[command] = {command, description, handler};
}

void CommandManager::Execute(const CommandContext &ctx, const std::string &line)
{
    if (line.empty() || line[0] != '/')
        return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg)
        args.push_back(arg);

    auto it = _commands.find(cmd);
    if (it != _commands.end())
    {
        // [Security] 인게임 채팅의 경우 GM 권한 체크 용이 (현재는 계획대로 통과)
        if (!ctx.FromServer() && !ctx.isGM)
        {
            // 실제 서비스 시 여기서 차단 가능
        }

        try
        {
            it->second.handler(ctx, args);
        } catch (...)
        {
            LOG_ERROR("[Command] Execution error: {}", cmd);
        }
    }
    else
    {
        if (ctx.FromServer())
            LOG_WARN("[Command] Unknown command: {}", cmd);
    }
}

} // namespace SimpleGame
