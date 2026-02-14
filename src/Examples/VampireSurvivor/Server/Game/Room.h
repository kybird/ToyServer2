#pragma once
#include "Entity/Player.h"
#include <mutex>
#include <unordered_map>
#include <vector>

#include "System/ITimer.h"

#include "Game/Effect/EffectManager.h"
#include "Game/GameConfig.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Game/WaveManager.h"

namespace System {
class IPacket;
class IStrand;
class IDispatcher;
class IFramework;
} // namespace System

namespace SimpleGame {
class UserDB;
class DamageEmitter;
class CombatManager;

struct RoomPerformanceProfile
{
    bool enableThrottledOverlap = true;
    int overlapThrottleFactor = 5;
    bool enableDistributedSync = true;
    int syncIntervalTicks = 5;
    int broadcastBatchSize = 500;
    bool enableInterestManagement = true;
    float interestRadius = 30.0f;
    bool logPerformance = true;
};

class Room : public System::ITimerListener, public std::enable_shared_from_this<Room>
{
    friend class CombatManager;
    friend class DamageEmitter;

public:
    Room(
        int roomId, std::shared_ptr<System::IFramework> framework, std::shared_ptr<System::IDispatcher> dispatcher,
        std::shared_ptr<System::ITimer> timer, std::shared_ptr<System::IStrand> strand, std::shared_ptr<UserDB> userDB
    );
    virtual ~Room() override;

    void Enter(const std::shared_ptr<Player> &player);
    void Leave(uint64_t sessionId);
    void OnPlayerReady(uint64_t sessionId);
    void BroadcastPacket(const System::IPacket &pkt, uint64_t excludeSessionId = 0);
    void BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects);
    void BroadcastDespawn(const std::vector<int32_t> &objectIds, const std::vector<int32_t> &pickerIds = {});
    void SendToPlayer(uint64_t sessionId, const System::IPacket &pkt);

    // Game Loop
    void Start();
    void Stop();
    void OnTimer(uint32_t timerId, void *pParam) override;
    void Update(float deltaTime);

    // AI Control
    void SetMonsterStrategy(const std::string &strategyName);

    // Getters
    int32_t GetId() const
    {
        return _roomId;
    }
    size_t GetPlayerCount();
    static size_t GetMaxPlayers()
    {
        return 4;
    }
    bool IsPlaying() const
    {
        return _gameStarted;
    }

    std::shared_ptr<Player> GetNearestPlayer(float x, float y);
    std::vector<std::shared_ptr<Monster>> GetMonstersInRange(float x, float y, float radius);

    std::shared_ptr<System::IStrand> GetStrand() const
    {
        return _strand;
    }

    void StartGame();
    void Reset();
    void HandleGameOver(bool isWin);

    void SetTitle(const std::string &title)
    {
        _title = title;
    }
    std::string GetTitle() const
    {
        return _title;
    }

    EffectManager &GetEffectManager()
    {
        return *_effectMgr;
    }
    ObjectManager &GetObjectManager()
    {
        return _objMgr;
    }
    SpatialGrid &GetSpatialGrid()
    {
        return _grid;
    }
    const SpatialGrid &GetSpatialGrid() const
    {
        return _grid;
    }
    float GetTotalRunTime() const
    {
        return _totalRunTime;
    }
    uint32_t GetServerTick() const
    {
        return _serverTick;
    }

    // Debug Commands
    void DebugAddExpToAll(int32_t exp);
    void DebugSpawnMonster(int32_t monsterId, int32_t count);
    void DebugToggleGodMode();

    bool CheckWinCondition() const;

private:
    void UpdatePhysics(float deltaTime, const std::vector<std::shared_ptr<GameObject>> &objects);
    void SyncNetwork();
    void BroadcastDebugState();
    void BroadcastDebugClear();

private:
    std::shared_ptr<System::IFramework> _framework;
    int _roomId;
    std::string _title;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;
    std::recursive_mutex _mutex;

    std::shared_ptr<System::ITimer> _timer;
    System::ITimer::TimerHandle _timerHandle = 0;
    std::shared_ptr<System::IStrand> _strand;

    ObjectManager _objMgr;
    std::vector<std::shared_ptr<GameObject>> _queryBuffer;

    SpatialGrid _grid{GameConfig::NEAR_GRID_CELL_SIZE};
    WaveManager _waveMgr;
    std::shared_ptr<System::IDispatcher> _dispatcher;
    std::shared_ptr<UserDB> _userDB;
    float _totalRunTime = 0.0f;
    uint32_t _serverTick = 0;
    float _debugBroadcastTimer = 0.0f;

    // Performance Monitoring
    float _lastPerfLogTime = 0.0f;
    float _totalUpdateSec = 0.0f;
    uint32_t _updateCount = 0;
    float _maxUpdateSec = 0.0f;

    bool _gameStarted = false;
    bool _isGameOver = false;
    std::unique_ptr<CombatManager> _combatMgr;
    std::unique_ptr<EffectManager> _effectMgr;

    RoomPerformanceProfile _perfProfile;
    bool _enableMonsterPhysics = false; // [New] Toggle for monster-to-monster physical collisions
};

} // namespace SimpleGame
