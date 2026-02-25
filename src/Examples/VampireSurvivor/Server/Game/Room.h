#pragma once
#include "Entity/Player.h"
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "System/ITimer.h"

#include "Game/Effect/EffectManager.h"
#include "Game/GameConfig.h"
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Game/TileMap.h"
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
    bool IsPlaying() const;

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
    float GetTotalRunTime() const;
    uint32_t GetServerTick() const;

    const TileMap *GetTileMap() const
    {
        return _tileMap;
    }

    // Debug Commands
    void DebugAddExpToAll(int32_t exp);
    void DebugSpawnMonster(int32_t monsterId, int32_t count);
    void DebugToggleGodMode();

    bool CheckWinCondition() const;

    // Cell-Based Navigation
    void ClearOccupancyMap()
    {
        _occupiedCells.clear();
    }
    bool IsCellOccupied(int x, int y) const
    {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
        return _occupiedCells.find(key) != _occupiedCells.end();
    }
    void OccupyCell(int x, int y)
    {
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y);
        _occupiedCells.insert(key);
    }

private:
    // [Serialization] Actual logic execution in Room's Strand
    void ExecuteEnter(const std::shared_ptr<Player> &player);
    void ExecuteLeave(uint64_t sessionId);
    void ExecuteOnPlayerReady(uint64_t sessionId);
    void ExecuteStartGame();
    void ExecuteReset();
    void ExecuteHandleGameOver(bool isWin);
    void ExecuteUpdate(float deltaTime);
    void ExecuteStop();
    void InternalClear();

    void UpdatePhysics(float deltaTime, const std::vector<std::shared_ptr<GameObject>> &objects);
    void SyncNetwork();
    void BroadcastDebugState();
    void BroadcastDebugClear();

private:
    std::shared_ptr<System::IFramework> _framework;
    int _roomId;
    std::string _title;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;
    std::unordered_set<uint64_t> _occupiedCells; // 1x1 점유 맵

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

    std::atomic<bool> _gameStarted{false}; // [Thread-Safe] Strand 바깥(GetRoomListHandler)에서도 읽힘
    bool _isGameOver = false;
    std::unique_ptr<CombatManager> _combatMgr;
    std::unique_ptr<EffectManager> _effectMgr;

    RoomPerformanceProfile _perfProfile;
    std::atomic<bool> _isStopping{false};
    std::atomic<bool> _isUpdating{false}; // [New] 업데이트 중복 실행 방지 플래그
    std::atomic<size_t> _playerCount{0};  // [New] Strand 바깥 호출을 위한 thread-safe 카운터
    bool _enableMonsterPhysics = false;   // [New] Toggle for monster-to-monster physical collisions

    const TileMap *_tileMap = nullptr;
};

} // namespace SimpleGame
