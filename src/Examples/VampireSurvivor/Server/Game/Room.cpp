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
    InternalClear();
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
    // [Fix] 이미 정지 중이면 중복 실행 방지
    if (_isStopping.exchange(true))
        return;

    LOG_INFO("Room {} STOP requested.", _roomId);

    // [Trap Fix] Framework가 살아있는 동안에는 워커가 이 작업을 받아줄 것임.
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self]()
            {
                self->ExecuteStop();
            }
        );
    }
    else
    {
        ExecuteStop();
    }
}

void Room::ExecuteStop()
{
    LOG_INFO("Room {} STOP initiated. (Players: {})", _roomId, _players.size());

    // 공통 정리 로직 호출
    InternalClear();

    LOG_INFO("Room {} STOP finished.", _roomId);
}

void Room::InternalClear()
{
    // [Safety] 이 함수는 소멸자에서도 호출되므로 shared_from_this를 절대 사용하면 안 됨.
    // 또한 중복 실행되어도 안전해야 함.

    if (_timer != nullptr && _timerHandle != 0)
    {
        _timer->CancelTimer(_timerHandle);
        _timerHandle = 0;
    }

    BroadcastDebugClear();

    _players.clear();
    _objMgr.Clear();
}

void Room::StartGame()
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self]()
            {
                self->ExecuteStartGame();
            }
        );
    }
    else
    {
        ExecuteStartGame();
    }
}

void Room::ExecuteStartGame()
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
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self]()
            {
                self->ExecuteReset();
            }
        );
    }
    else
    {
        ExecuteReset();
    }
}

void Room::ExecuteReset()
{
    // ObjectManager 및 Grid 클리어
    _objMgr.Clear();
    _grid.HardClear();

    _waveMgr.Reset();

    BroadcastDebugClear();

    _gameStarted = false;
    _isGameOver = false;
    _isStopping.store(false); // [Fix] 방을 다시 사용할 수 있도록 부활
    _totalRunTime = 0.0f;
    _serverTick = 0;

    _lastPerfLogTime = 0.0f;
    _totalUpdateSec = 0.0f;
    _updateCount = 0;
    _maxUpdateSec = 0.0f;
    _isUpdating.store(false);
    _playerCount.store(0); // [Thread-Safe] atomic 카운터 초기화

    LOG_INFO("Room {} reset complete.", _roomId);
}

void Room::OnTimer(uint32_t timerId, void *pParam)
{
    (void)timerId;
    (void)pParam;

    // [DESIGN NOTE] CAS는 반드시 Post() 호출 전에 수행해야 한다.
    //
    // 이유: 람다 내부로 CAS를 옮기면 Post() 자체를 차단할 수 없어
    //       Strand 큐에 작업이 무한히 쌓이는 큐 폭주(queue flooding)가 발생한다.
    //
    // 동작 원리:
    //   1. OnTimer(로직 스레드) → CAS(false→true) 성공 → Post(람다)
    //   2. Strand가 람다 실행 → Update() → RAII Guard가 flag를 false로 복원
    //   3. 다음 OnTimer 호출 시:
    //      - Update 완료 전이면 → CAS 실패 → 정당한 프레임 스킵
    //      - Update 완료 후이면 → CAS 성공 → 정상 실행
    //
    // 주의: 이 CAS를 절대 람다 내부로 이동하지 말 것.
    bool expected = false;
    if (!_isUpdating.compare_exchange_strong(expected, true))
    {
        return;
    }

    if (_strand != nullptr)
    {
        auto self = shared_from_this();
        _strand->Post(
            [self]()
            {
                self->Update(GameConfig::TICK_INTERVAL_SEC);
            }
        );
    }
    else
    {
        Update(GameConfig::TICK_INTERVAL_SEC);
    }
}

void Room::Update(float deltaTime)
{
    // [RAII] 예외 발생 시에도 반드시 플래그 해제 보장
    // OnTimer에서 CAS로 이미 true 설정했으므로 추가 store(true) 불필요
    struct UpdateGuard
    {
        std::atomic<bool> &flag;
        ~UpdateGuard()
        {
            flag.store(false);
        }
    } guard{_isUpdating};

    ExecuteUpdate(deltaTime);
}

void Room::Enter(const std::shared_ptr<Player> &player)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, player]()
            {
                self->ExecuteEnter(player);
            }
        );
    }
    else
    {
        ExecuteEnter(player);
    }
}

void Room::ExecuteEnter(const std::shared_ptr<Player> &player)
{
    // [Fix] 기존에는 1인 테스트를 위해 신규 입장 시 무조건 방을 리셋했으나,
    // 멀티플레이어 환경(스트레스 테스트 등)을 위해 해당 로직을 제거함.
    {
        // 특정 세션이 이미 존재할 경우에 대한 처리는 필요할 수 있으나,
        // 전체 플레이어를 지우는 것은 멀티 환경에서 치명적임.
    }

    _players[player->GetSessionId()] = player;
    _playerCount++; // [Thread-Safe] atomic 카운터 증가
    player->SetRoomId(_roomId);

    LOG_INFO("Player {} connecting to Room {}. Loading Data...", player->GetSessionId(), _roomId);

    int32_t userId = static_cast<int32_t>(player->GetSessionId());
    auto self = shared_from_this();

    if (_userDB != nullptr)
    {
        _userDB->GetUserSkills(
            userId,
            [self, player](const std::vector<std::pair<int, int>> &skills)
            {
                // [Fix] DB 콜백에서 직접 멤버에 접근하지 않고 Strand로 Post하여 직렬화 보장
                self->_strand->Post(
                    [self, player, skills]()
                    {
                        auto it = self->_players.find(player->GetSessionId());
                        if (it == self->_players.end() || it->second != player)
                        {
                            LOG_WARN("Player {} disconnected while loading.", player->GetSessionId());
                            return;
                        }

                        player->ApplySkills(skills, self.get());
                        LOG_INFO("Applied {} skills to Player {}", skills.size(), player->GetSessionId());

                        self->_objMgr.AddObject(player);
                        self->_grid.Add(player);

                        LOG_INFO(
                            "Player {} entered Room {}. Total Players: {}",
                            player->GetSessionId(),
                            self->_roomId,
                            self->_players.size()
                        );
                        LOG_INFO(
                            "Player {} entered Room {}. Waiting for C_GAME_READY.",
                            player->GetSessionId(),
                            self->_roomId
                        );

                        const auto *pTmpl = DataManager::Instance().GetPlayerTemplate(1);
                        if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
                        {
                            player->AddDefaultSkills(pTmpl->defaultSkills, self.get());
                            LOG_INFO(
                                "Applied {} default skills to Player {}",
                                pTmpl->defaultSkills.size(),
                                player->GetSessionId()
                            );
                        }
                        if (!self->_gameStarted)
                        {
                            self->StartGame();
                        }
                    }
                );
            }
        );
    }
    else
    {
        const auto *pTmpl = DataManager::Instance().GetPlayerTemplate(1);
        if (pTmpl != nullptr && !pTmpl->defaultSkills.empty())
        {
            player->AddDefaultSkills(pTmpl->defaultSkills, this);
        }

        _objMgr.AddObject(player);
        _grid.Add(player);

        if (!_gameStarted)
        {
            StartGame();
        }
    }
}

void Room::OnPlayerReady(uint64_t sessionId)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, sessionId]()
            {
                self->ExecuteOnPlayerReady(sessionId);
            }
        );
    }
    else
    {
        ExecuteOnPlayerReady(sessionId);
    }
}

void Room::ExecuteOnPlayerReady(uint64_t sessionId)
{
    auto it = _players.find(sessionId);
    if (it == _players.end())
    {
        LOG_WARN("ExecuteOnPlayerReady: Player {} not found in room {}", sessionId, _roomId);
        return;
    }

    auto player = it->second;
    player->SetReady(true);
    LOG_INFO("Player {} is ready in Room {}", sessionId, _roomId);

    // [Fix] 확보된 세션으로 인벤토리 상태 강제 동기화 (기본 무기 표시 누락 방지)
    player->SyncInventory(this);

    if (!_gameStarted)
    {
        ExecuteStartGame();
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
            info->set_look_left(obj->GetLookLeft());

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
}

void Room::Leave(uint64_t sessionId)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, sessionId]()
            {
                self->ExecuteLeave(sessionId);
            }
        );
    }
    else
    {
        ExecuteLeave(sessionId);
    }
}

void Room::ExecuteLeave(uint64_t sessionId)
{
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
        _playerCount--; // [Thread-Safe] atomic 카운터 감소
        LOG_INFO("Player {} left Room {}", sessionId, _roomId);

        if (_players.empty())
        {
            if (_roomId != 1)
            {
                ExecuteStop();
                ExecuteReset();
            }
            else
            {
                // [Fix] 1번 방은 엔진(Stop)은 유지하되 다음 플레이어를 위해 리셋만 수행
                ExecuteReset();
            }
        }
    }
}

size_t Room::GetPlayerCount()
{
    // [Thread-Safe] Strand 바깥(GetRoomListHandler 등)에서도 호출되므로 atomic 카운터 사용
    return _playerCount.load();
}

void Room::HandleGameOver(bool isWin)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, isWin]()
            {
                self->ExecuteHandleGameOver(isWin);
            }
        );
    }
    else
    {
        ExecuteHandleGameOver(isWin);
    }
}

void Room::ExecuteHandleGameOver(bool isWin)
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
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self]()
            {
                for (auto &[sid, p] : self->_players)
                {
                    p->SetGodMode(!p->IsGodMode());
                    LOG_INFO("Debug: GodMode toggled for Player {} -> {}", sid, p->IsGodMode());
                }
            }
        );
    }
}

void Room::SetMonsterStrategy(const std::string &strategyName)
{
    auto self = shared_from_this();
    if (_strand)
    {
        _strand->Post(
            [self, strategyName]()
            {
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

                auto objects = self->_objMgr.GetAllObjects();
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
        );
    }
}

bool Room::IsPlaying() const
{
    return _gameStarted;
}

float Room::GetTotalRunTime() const
{
    return _totalRunTime;
}

uint32_t Room::GetServerTick() const
{
    return _serverTick;
}

} // namespace SimpleGame
