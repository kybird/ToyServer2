#pragma once
#include "Entity/Player.h"
#include <mutex>
#include <unordered_map>
#include <vector>

#include "System/ITimer.h"

#include "Game/Effect/EffectManager.h"
#include "Game/GameConfig.h" // [Added] For NEAR_GRID_CELL_SIZE
#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Game/WaveManager.h"

namespace System {
class IPacket;
class IStrand;
class IDispatcher;
} // namespace System

namespace SimpleGame {
class UserDB;
class DamageEmitter;
class CombatManager;

struct RoomPerformanceProfile
{
    // 1. Overlap Resolution Optimization
    bool enableThrottledOverlap = true;
    int overlapThrottleFactor = 5;

    // 2. Broadcast Optimization
    bool enableDistributedSync = true;
    int syncIntervalTicks = 5;
    int broadcastBatchSize = 500;

    // 3. Interest Management (Field of View Cutting)
    bool enableInterestManagement = true;
    float interestRadius = 30.0f;

    // 4. Debug/Stress Test
    bool logPerformance = true;
};

class Room : public System::ITimerListener, public std::enable_shared_from_this<Room>
{
    friend class CombatTest_ProjectileHitsMonster_Test;
    friend class CombatTest_MonsterDies_Test;
    friend class CombatTest_MonsterContactsPlayer_Test;
    friend class SwarmPerformanceTest_StressTest500Monsters_Test;
    friend class CombatTest_LinearEmitterHitsNearestMonster_Test;
    friend class CombatTest_LinearEmitterRespectsLifetime_Test;
    friend class CombatTest_MonsterKnockback_Test;
    friend class CombatTest_LinearEmitterSpawnsProjectile_Test;
    friend class CombatManager;
    friend class DamageEmitter;

public:
    Room(
        int roomId, std::shared_ptr<System::IDispatcher> dispatcher, std::shared_ptr<System::ITimer> timer,
        std::shared_ptr<System::IStrand> strand, std::shared_ptr<UserDB> userDB
    );
    virtual ~Room() override;

    void Enter(const std::shared_ptr<Player> &player);
    void Leave(uint64_t sessionId);
    void OnPlayerReady(uint64_t sessionId); // Called when client finishes loading
    void BroadcastPacket(const System::IPacket &pkt);
    void BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects);
    void BroadcastDespawn(const std::vector<int32_t> &objectIds, const std::vector<int32_t> &pickerIds = {});
    void SendToPlayer(uint64_t sessionId, const System::IPacket &pkt);

    // Game Loop
    void Start();
    void Stop();
    void OnTimer(uint32_t timerId, void *pParam) override;
    void Update(float deltaTime);

    int GetId() const
    {
        return _roomId;
    }
    size_t GetPlayerCount();
    static size_t GetMaxPlayers()
    {
        return 4;
    } // TODO: Make configurable
    bool IsPlaying() const
    {
        return _gameStarted;
    }

    std::shared_ptr<Player> GetNearestPlayer(float x, float y);
    std::vector<std::shared_ptr<Monster>> GetMonstersInRange(float x, float y, float radius);

    // [Optimization] Added velocity check and max neighbors limit
    Vector2 GetSeparationVector(
        float x, float y, float radius, int32_t excludeId, const Vector2 &velocity = Vector2::Zero(),
        int maxNeighbors = 6
    );

    std::shared_ptr<System::IStrand> GetStrand() const
    {
        return _strand;
    }

    void StartGame(); // Start wave manager when first player enters
    void Reset();     // Reset room state when all players leave
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

    float GetTotalRunTime() const
    {
        return _totalRunTime;
    }

    // Debug Commands
    void DebugAddExpToAll(int32_t exp);
    void DebugSpawnMonster(int32_t monsterId, int32_t count);
    void DebugToggleGodMode();

    // void HandleGameOver(bool isWin); (Removed duplicate)
    void UpdateObject(
        const std::shared_ptr<GameObject> &obj, float deltaTime, std::vector<Protocol::ObjectPos> &monsterMoves,
        std::vector<Protocol::ObjectPos> &playerMoves
    );
    void BroadcastPlayerMoves(const std::vector<Protocol::ObjectPos> &playerMoves);
    void SendPlayerAcks();
    bool CheckWinCondition() const;

private:
    int _roomId;
    std::string _title;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players; // SessionID -> Player
    std::recursive_mutex _mutex;

    std::shared_ptr<System::ITimer> _timer;
    System::ITimer::TimerHandle _timerHandle = 0;
    std::shared_ptr<System::IStrand> _strand;

    // Game Logic Components
    ObjectManager _objMgr;
    SpatialGrid _grid{GameConfig::NEAR_GRID_CELL_SIZE}; // [Optimized] Use correct cell size
    WaveManager _waveMgr;
    std::shared_ptr<System::IDispatcher> _dispatcher;
    std::shared_ptr<UserDB> _userDB;
    float _totalRunTime = 0.0f;
    uint32_t _serverTick = 0;
    float _debugBroadcastTimer = 0.0f; // Stage 0 Verification Timer

    // Performance Monitoring
    float _lastPerfLogTime = 0.0f;
    float _totalUpdateSec = 0.0f;
    uint32_t _updateCount = 0;
    float _maxUpdateSec = 0.0f;

public:
    uint32_t GetServerTick() const
    {
        return _serverTick;
    }

private:
    bool _gameStarted = false; // Track if game has started
    bool _isGameOver = false;  // Track if game is over
    std::unique_ptr<CombatManager> _combatMgr;
    std::unique_ptr<EffectManager> _effectMgr;

    // Standard Performance Strategies
    void ResolveMonsterOverlap(const std::vector<std::shared_ptr<GameObject>> &objects);
    void SyncMonsterStates(const std::vector<Protocol::ObjectPos> &monsterMoves);

    RoomPerformanceProfile _perfProfile;
};

} // namespace SimpleGame
