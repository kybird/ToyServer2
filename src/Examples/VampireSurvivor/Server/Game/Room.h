#pragma once
#include "Entity/Player.h"
#include "Protocol.h"
#include "System/ILog.h"
#include <mutex>
#include <unordered_map>
#include <vector>

#include "System/ITimer.h"

#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include "Game/WaveManager.h"


namespace System {
class IPacket;
class IStrand;
} // namespace System

namespace SimpleGame {
class UserDB;

class Room : public System::ITimerListener, public std::enable_shared_from_this<Room>
{
    friend class CombatTest_ProjectileHitsMonster_Test;
    friend class CombatTest_MonsterDies_Test;
    friend class SwarmPerformanceTest_StressTest500Monsters_Test;
public:
    Room(
        int roomId, std::shared_ptr<System::ITimer> timer, std::shared_ptr<System::IStrand> strand,
        std::shared_ptr<UserDB> userDB
    );
    ~Room();

    void Enter(std::shared_ptr<Player> player);
    void Leave(uint64_t sessionId);
    void BroadcastPacket(const System::IPacket &pkt);
    void BroadcastSpawn(const std::vector<std::shared_ptr<GameObject>> &objects);
    void BroadcastDespawn(const std::vector<int32_t> &objectIds);

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
    size_t GetMaxPlayers() const
    {
        return 4;
    } // TODO: Make configurable
    bool IsPlaying() const
    {
        return _gameStarted;
    }

    std::shared_ptr<Player> GetNearestPlayer(float x, float y);

    std::shared_ptr<System::IStrand> GetStrand() const
    {
        return _strand;
    }

    void StartGame(); // Start wave manager when first player enters
    void Reset();     // Reset room state when all players leave

private:
    int _roomId;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players; // SessionID -> Player
    std::recursive_mutex _mutex;

    std::shared_ptr<System::ITimer> _timer;
    System::ITimer::TimerHandle _timerHandle = 0;
    std::shared_ptr<System::IStrand> _strand;

    // Game Logic Components
    ObjectManager _objMgr;
    SpatialGrid _grid{2000.0f}; // [Phase 1] Large cell for Full Broadcast
    WaveManager _waveMgr;
    std::shared_ptr<UserDB> _userDB;
    float _totalRunTime = 0.0f;
    bool _gameStarted = false; // Track if game has started
};

} // namespace SimpleGame
