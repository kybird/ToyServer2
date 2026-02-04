#include "Game/Room.h"
#include "Core/UserDB.h"
#include "Entity/AI/Movement/FluidStackingStrategy.h"
#include "Entity/AI/Movement/SmartFlockingStrategy.h"
#include "Entity/AI/Movement/StrictSeparationStrategy.h"
#include "Entity/AI/Movement/SurroundingFlockingStrategy.h"
#include "Entity/Monster.h"
#include "Entity/Player.h"
#include "Entity/Projectile.h"
#include "Game/CombatManager.h"
#include "Game/DamageEmitter.h"
#include "Game/GameConfig.h"
#include "GamePackets.h"
#include "Protocol/game.pb.h"
#include "System/Dispatcher/IDispatcher.h"
#include "System/IFramework.h"
#include "System/Thread/IStrand.h"

namespace SimpleGame {

Room::Room(
    int roomId, std::shared_ptr<System::IFramework> framework, std::shared_ptr<System::IDispatcher> dispatcher,
    std::shared_ptr<System::ITimer> timer, std::shared_ptr<System::IStrand> strand, std::shared_ptr<UserDB> userDB
)
    : _roomId(roomId), _framework(std::move(framework)), _timer(std::move(timer)), _strand(std::move(strand)),
      _waveMgr(_objMgr, _grid, roomId), _dispatcher(std::move(dispatcher)), _userDB(std::move(userDB))
{
    _combatMgr = std::make_unique<CombatManager>();
    _effectMgr = std::make_unique<EffectManager>();
}

Room::~Room()
{
    Stop();
}

void Room::Start()
{
    std::cout << "[DEBUG] Room::Start(" << _roomId << ")\n";
    if (_timer != nullptr)
    {
        _timerHandle = _timer->SetInterval(1, GameConfig::TICK_INTERVAL_MS, this);
        std::cout << "[DEBUG] Room " << _roomId << " Game Loop Started (" << GameConfig::TPS << " TPS, "
                  << GameConfig::TICK_INTERVAL_MS << "ms). TimerHandle: " << _timerHandle << "\n";
    }
    else
    {
        std::cerr << "[ERROR] Room " << _roomId << " has NO TIMER!\n";
    }
    LOG_INFO("Room {} created. Waiting for players...", _roomId);
}

void Room::Stop()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    LOG_INFO("Room {} STOP initiated. (Players: {})", _roomId, _players.size());

    if (_timer != nullptr && _timerHandle != 0)
    {
        _timer->CancelTimer(_timerHandle);
        _timerHandle = 0;
    }

    BroadcastDebugClear();

    _players.clear();
    _objMgr.Clear();

    LOG_INFO("Room {} STOP finished.", _roomId);
}

void Room::StartGame()
{
    if (_gameStarted)
        return;

    _gameStarted = true;

    if (_timerHandle == 0)
    {
        Start();
    }

    _waveMgr.Start();
    LOG_INFO("Game started in Room {}! Wave spawning begins.", _roomId);
}

void Room::Reset()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    // ObjectManager 및 Grid 클리어
    _objMgr.Clear();
    _grid.HardClear();

    _waveMgr.Reset();

    BroadcastDebugClear();

    _gameStarted = false;
    _isGameOver = false;
    _totalRunTime = 0.0f;
    _serverTick = 0;

    _lastPerfLogTime = 0.0f;
    _totalUpdateSec = 0.0f;
    _updateCount = 0;
    _maxUpdateSec = 0.0f;

    LOG_INFO("Room {} has been reset. Ready for new game.", _roomId);
}

void Room::OnTimer(uint32_t timerId, void *pParam)
{
    (void)timerId;
    (void)pParam;
    _serverTick++;
    if (_strand != nullptr)
    {
        _strand->Post(
            [this]()
            {
                this->Update(GameConfig::TICK_INTERVAL_SEC);
            }
        );
    }
    else
    {
        Update(GameConfig::TICK_INTERVAL_SEC);
    }
}

void Room::Enter(const std::shared_ptr<Player> &player)
{
    // [Safety] 과부하로 인한 Disconnect 지연 시, 재접속하면 기존 좀비 세션이 남아있을 수 있음.
    // 여기서는 테스트 편의를 위해(혹은 1인 방 가정) 기존 플레이어를 정리함.
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (!_players.empty())
        {
            LOG_WARN("Room::Enter - Clearing {} stale players for new connection.", _players.size());
            for (auto &pair : _players)
            {
                // ObjectManager에서 제거해야 Rebuild 시 포함되지 않음
                _objMgr.RemoveObject(pair.second->GetId());
            }
            _players.clear();

            // 재입장 시 웨이브 리셋
            Reset();
        }
    }

    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _players[player->GetSessionId()] = player;
        player->SetRoomId(_roomId);
    }

    LOG_INFO("Player {} connecting to Room {}. Loading Data...", player->GetSessionId(), _roomId);

    int32_t userId = static_cast<int32_t>(player->GetSessionId());
    auto self = shared_from_this();

    if (_userDB != nullptr)
    {
        _userDB->GetUserSkills(
            userId,
            [self, player](const std::vector<std::pair<int, int>> &skills)
            {
                std::lock_guard<std::recursive_mutex> lock(self->_mutex);

                auto it = self->_players.find(player->GetSessionId());
                if (it == self->_players.end() || it->second != player)
                {
                    LOG_WARN("Player {} disconnected while loading.", player->GetSessionId());
                    return;
                }

                player->ApplySkills(skills);
                LOG_INFO("Applied {} skills to Player {}", skills.size(), player->GetSessionId());

                self->_objMgr.AddObject(player);
                self->_grid.Add(player);

                LOG_INFO(
                    "Player {} entered Room {}. Total Players: {}",
                    player->GetSessionId(),
                    self->_roomId,
                    self->_players.size()
                );
                LOG_INFO("Player {} entered Room {}. Waiting for C_GAME_READY.", player->GetSessionId(), self->_roomId);

                const auto *pTmpl = DataManager::Instance().GetPlayerTemplate(1);
                if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
                {
                    player->AddDefaultSkills(pTmpl->defaultSkills);
                    LOG_INFO(
                        "Applied {} default skills to Player {}", pTmpl->defaultSkills.size(), player->GetSessionId()
                    );
                }
            }
        );
    }
    else
    {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        const auto *pTmpl = DataManager::Instance().GetPlayerTemplate(1);
        if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
        {
            player->AddDefaultSkills(pTmpl->defaultSkills);
        }

        _objMgr.AddObject(player);
        _grid.Add(player);
        LOG_INFO("Player {} entered Room {} (No DB).", player->GetSessionId(), _roomId);
    }
}

void Room::OnPlayerReady(uint64_t sessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto it = _players.find(sessionId);
    if (it == _players.end())
    {
        LOG_WARN("OnPlayerReady: Player {} not found in room {}", sessionId, _roomId);
        return;
    }

    auto player = it->second;
    player->SetReady(true);
    LOG_INFO("Player {} is ready in Room {}", sessionId, _roomId);

    if (!_gameStarted)
    {
        StartGame();
    }

    auto allObjects = _objMgr.GetAllObjects();
    if (!allObjects.empty())
    {
        Protocol::S_SpawnObject existingObjects;
        existingObjects.set_server_tick(_serverTick);
        for (const auto &obj : allObjects)
        {
            auto *info = existingObjects.add_objects();
            info->set_object_id(obj->GetId());
            info->set_type(obj->GetType());
            info->set_x(obj->GetX());
            info->set_y(obj->GetY());
            info->set_hp(obj->GetHp());
            info->set_max_hp(obj->GetMaxHp());
            info->set_state(obj->GetState());
            info->set_vx(obj->GetVX());
            info->set_vy(obj->GetVY());

            if (obj->GetType() == Protocol::ObjectType::MONSTER)
            {
                auto monster = std::dynamic_pointer_cast<Monster>(obj);
                if (monster != nullptr)
                {
                    info->set_type_id(monster->GetMonsterTypeId());
                }
            }
            else if (obj->GetType() == Protocol::ObjectType::PROJECTILE)
            {
                auto proj = std::dynamic_pointer_cast<Projectile>(obj);
                if (proj != nullptr)
                {
                    info->set_type_id(proj->GetTypeId());
                }
            }
        }

        if (existingObjects.objects_size() > 0)
        {
            S_SpawnObjectPacket packet(existingObjects);
            SendToPlayer(sessionId, packet);
            LOG_INFO("Sent {} existing objects to ready player {}", existingObjects.objects_size(), sessionId);
        }
    }

    Protocol::S_SpawnObject newPlayerSpawn;
    newPlayerSpawn.set_server_tick(_serverTick);
    auto *info = newPlayerSpawn.add_objects();
    info->set_object_id(player->GetId());
    info->set_type(Protocol::ObjectType::PLAYER);
    info->set_x(player->GetX());
    info->set_y(player->GetY());
    info->set_hp(player->GetHp());
    info->set_max_hp(player->GetMaxHp());
    info->set_state(Protocol::ObjectState::IDLE);

    BroadcastPacket(S_SpawnObjectPacket(std::move(newPlayerSpawn)), sessionId);
    LOG_INFO("Broadcasted ready player {} spawn to other players in room", sessionId);

    for (float r = 10.0f; r <= 40.0f; r += 10.0f)
    {
        Protocol::S_DebugDrawBox box;
        box.set_x(0);
        box.set_y(0);
        box.set_w(r * 2.0f);
        box.set_h(r * 2.0f);
        box.set_duration(9999.0f);
        uint32_t color = 0xFFFFFFFF;
        if (std::abs(r - 10.0f) < 0.1f)
            color = 0xFFFF0000;
        else if (std::abs(r - 20.0f) < 0.1f)
            color = 0xFF00FF00;
        else if (std::abs(r - 30.0f) < 0.1f)
            color = 0xFF0000FF;

        box.set_color_hex(color);

        S_DebugDrawBoxPacket pkt(box);
        SendToPlayer(sessionId, pkt);
    }
}

void Room::Leave(uint64_t sessionId)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    auto it = _players.find(sessionId);
    if (it != _players.end())
    {
        auto player = it->second;
        player->SetRoomId(0);

        _grid.Remove(player);
        _objMgr.RemoveObject(player->GetId());

        Protocol::S_DespawnObject despawn;
        despawn.add_object_ids(player->GetId());
        BroadcastPacket(S_DespawnObjectPacket(std::move(despawn)));

        _players.erase(it);
        LOG_INFO("Player {} left Room {}", sessionId, _roomId);

        if (_players.empty())
        {
            Stop();
            LOG_INFO("Room {} is empty. Stopping game loop timer.", _roomId);
            Reset();
        }
    }
}

size_t Room::GetPlayerCount()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    return _players.size();
}

void Room::HandleGameOver(bool isWin)
{
    if (_isGameOver)
        return;

    _isGameOver = true;
    LOG_INFO("Game Over in Room {} (Win: {})", _roomId, isWin);

    Protocol::S_GameOver msg;
    msg.set_survived_time_ms(static_cast<int64_t>(_totalRunTime * 1000));
    msg.set_is_win(isWin);

    BroadcastPacket(S_GameOverPacket(std::move(msg)));
}

bool Room::CheckWinCondition() const
{
    return _waveMgr.IsAllWavesComplete() && _objMgr.GetAliveMonsterCount() == 0;
}

void Room::DebugToggleGodMode()
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    for (auto &[sid, p] : _players)
    {
        p->SetGodMode(!p->IsGodMode());
        LOG_INFO("Debug: GodMode toggled for Player {} -> {}", sid, p->IsGodMode());
    }
}

void Room::SetMonsterStrategy(const std::string &strategyName)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);

    std::shared_ptr<IMovementStrategy> strategy;
    if (strategyName == "smart")
    {
        strategy = std::make_shared<SmartFlockingStrategy>();
    }
    else if (strategyName == "fluid")
    {
        strategy = std::make_shared<FluidStackingStrategy>();
    }
    else if (strategyName == "strict")
    {
        strategy = std::make_shared<StrictSeparationStrategy>();
    }
    else if (strategyName == "surround")
    {
        strategy = std::make_shared<SurroundingFlockingStrategy>();
    }
    else
    {
        LOG_WARN("Unknown strategy name: {}", strategyName);
        return;
    }

    auto objects = _objMgr.GetAllObjects();
    int count = 0;
    for (auto &obj : objects)
    {
        if (obj->GetType() == Protocol::ObjectType::MONSTER)
        {
            auto monster = std::dynamic_pointer_cast<Monster>(obj);
            if (monster)
            {
                monster->SetMovementStrategy(strategy);
                count++;
            }
        }
    }
    LOG_INFO("Changed strategy to {} for {} monsters", strategyName, count);
}

} // namespace SimpleGame
