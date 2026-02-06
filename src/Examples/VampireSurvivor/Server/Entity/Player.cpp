#include "Entity/Player.h"
#include "Core/DataManager.h"
#include "Entity/Monster.h"
#include "Entity/PlayerInventory.h"
#include "Game/DamageEmitter.h"
#include "Game/GameConfig.h"
#include "Game/LevelUpManager.h"
#include "Game/ObjectManager.h"
#include "Game/Room.h"
#include "GamePackets.h"

namespace SimpleGame {

Player::Player(int32_t gameId, uint64_t sessionId)
    : GameObject(gameId, Protocol::ObjectType::PLAYER), _sessionId(sessionId)
{
    _inventory = std::make_unique<PlayerInventory>();
}

Player::Player() : GameObject(0, Protocol::ObjectType::PLAYER), _sessionId(0)
{
    _inventory = std::make_unique<PlayerInventory>();
}

Player::~Player() = default;

void Player::Initialize(int32_t gameId, uint64_t sessionId, int32_t hp, float speed)
{
    _id = gameId;
    _sessionId = sessionId;
    _baseMaxHp = hp;
    _maxHp = hp;
    _hp = hp;
    _radius = GameConfig::PLAYER_COLLISION_RADIUS; // Lag Compensation: Visual(0.5) - BufferDelay
    _speed = speed;
    _classId = 0;
    _name.clear();
    _vx = _vy = 0;
    _x = _y = 0;
    _lastInputTick = 0;
    _exp = 0;
    _maxExp = 100;
    _level = 1;

    _invincibleUntil = 0.0f;
    _isLevelingUp = false;

    // Initialize inventory
    if (_inventory == nullptr)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }
    _pendingLevelUpOptions.clear();
    _slowedMonsterIds.clear();
}

void Player::ApplyInput(uint32_t clientTick, int32_t dx, int32_t dy)
{
    _lastInputTick = clientTick;

    if (IsControlDisabled() || _isLevelingUp)
        return;

    Vector2 dir(static_cast<float>(dx), static_cast<float>(dy));

    if (dir.IsZero())
    {
        SetVelocity(0, 0);
        SetState(Protocol::IDLE);
        return;
    }

    dir.Normalize();
    _facingDirX = dir.x;
    _facingDirY = dir.y;

    // [이동 동기화] 패시브 아이템 등으로 인한 이동 속도 배율을 적용합니다.
    float finalSpeed = _speed * GetMovementSpeedMultiplier();
    SetVelocity(dir.x * finalSpeed, dir.y * finalSpeed);

    SetState(Protocol::MOVING);
    // LOG_DEBUG(
    //     "[SetInput] Player={} ClientTick={} Dir=({:.2f}, {:.2f}) Vel=({:.2f},{:.2f})",
    //     GetId(),
    //     clientTick,
    //     dir.x,
    //     dir.y,
    //     GetVX(),
    //     GetVY()
    // );
}

void Player::TakeDamage(int32_t damage, Room *room)
{
    if (IsDead())
        return;

    if (_isLevelingUp)
        return;

    // 무적 상태 체크
    if (IsInvincible(room->GetTotalRunTime()) || _godMode)
        return;

    // 무적 시간 적용
    SetInvincible(room->GetTotalRunTime() + GameConfig::PLAYER_INVINCIBLE_DURATION);

    _hp -= damage;
    if (_hp <= 0)
    {
        _hp = 0;
        SetState(Protocol::ObjectState::DEAD);

        // Notify client of death
        if (room != nullptr)
        {
            Protocol::S_PlayerDead msg;
            msg.set_player_id(_id);
            S_PlayerDeadPacket pkt(std::move(msg));
            room->BroadcastPacket(pkt);
        }
    }
}

bool Player::IsDead() const
{
    return _state == Protocol::ObjectState::DEAD;
}

void Player::Reset()
{
    _sessionId = 0;
    _name.clear();
    _classId = 0;
    _hp = 0;
    _maxHp = 0;
    _id = 0;
    _state = Protocol::ObjectState::IDLE;
    _facingDirX = 1.0f;
    _facingDirY = 0.0f;
    _exp = 0;
    _maxExp = 100;
    _maxExp = 100;
    _level = 1;
    _invincibleUntil = 0.0f;
    _isLevelingUp = false;
    _isReady = false;
    _godMode = false;

    // Reset inventory
    if (_inventory != nullptr)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }
    _pendingLevelUpOptions.clear();
    _slowedMonsterIds.clear();
    _emitters.clear(); // [Fix] Clear emitters to prevent reuse of stale emitters in object pooling
}

uint64_t Player::GetSessionId() const
{
    return _sessionId;
}

void Player::SetName(const std::string &name)
{
    _name = name;
}

const std::string &Player::GetName() const
{
    return _name;
}

void Player::SetClassId(int32_t classId)
{
    _classId = classId;
}

int32_t Player::GetClassId() const
{
    return _classId;
}

void Player::SetRoomId(int roomId)
{
    _currentRoomId = roomId;
}

int Player::GetRoomId() const
{
    return _currentRoomId;
}

Vector2 Player::GetFacingDirection() const
{
    return Vector2(_facingDirX, _facingDirY);
}

void Player::ApplySkills(const std::vector<std::pair<int, int>> &skills)
{
    // Example: Skill ID 101 = Max HP
    for (const auto &skill : skills)
    {
        int id = skill.first;
        int lvl = skill.second;

        if (id == GameConfig::SKILL_ID_MAX_HP_BONUS)
        {
            // +10 HP per level
            _maxHp += 10 * lvl;
            _hp = _maxHp;
        }
        // Add more skills here
    }
}

uint32_t Player::GetLastProcessedClientTick() const
{
    return _lastInputTick;
}

int32_t Player::GetExp() const
{
    return _exp;
}

int32_t Player::GetMaxExp() const
{
    return _maxExp;
}

int32_t Player::GetLevel() const
{
    return _level;
}

void Player::AddExp(int32_t amount, Room *room)
{
    if (_isLevelingUp)
        return;

    _exp += amount;

    bool leveledUp = false;
    while (_exp >= _maxExp)
    {
        _exp -= _maxExp;
        _level++;
        _maxExp = GameConfig::EXP_BASE + (_level - 1) * GameConfig::EXP_PER_LEVEL_INCREMENT;
        LOG_INFO("Player {} leveled up to {}!", GetId(), _level);
        leveledUp = true;
    }

    if (_sessionId != 0 && room != nullptr)
    {
        Protocol::S_ExpChange expMsg;
        expMsg.set_current_exp(_exp);
        expMsg.set_max_exp(_maxExp);
        expMsg.set_level(_level);

        S_ExpChangePacket pkt(std::move(expMsg));
        room->SendToPlayer(_sessionId, pkt);

        if (leveledUp)
        {
            LevelUpManager levelUpMgr;
            auto options = levelUpMgr.GenerateOptions(this);
            if (options.empty())
            {
                LOG_WARN("[Player] Level up triggered but no options available for player {}", GetId());
                return;
            }

            EnterLevelUpState(room);
            SetPendingLevelUpOptions(options);

            Protocol::S_LevelUpOption optionMsg;
            optionMsg.set_timeout_seconds(GameConfig::LEVEL_UP_TIMEOUT_SEC);
            optionMsg.set_slow_radius(10.0f); // Match the logic in Update()
            for (const auto &opt : options)
            {
                auto *protoOpt = optionMsg.add_options();
                protoOpt->set_option_id(opt.optionId);
                protoOpt->set_skill_id(opt.itemId);
                protoOpt->set_name(opt.name);
                protoOpt->set_desc(opt.desc);
                protoOpt->set_is_new(opt.isNew);
                protoOpt->set_item_type(
                    opt.type == LevelUpOptionType::WEAPON ? Protocol::WEAPON_TYPE : Protocol::PASSIVE_TYPE
                );
            }

            S_LevelUpOptionPacket optPkt(std::move(optionMsg));
            room->SendToPlayer(_sessionId, optPkt);

            LOG_INFO("[Player] Sent level-up options to player {}", GetId());
        }
    }
}

void Player::Update(float dt, Room *room)
{
    if (_isLevelingUp)
    {
        // [이동 동기화 수정보완] 레벨업 상태에서의 슬로우 오라 부하 최적화
        // 매 몬스터마다 BroadcastPacket을 호출하면 네트워크 큐 오버플로우 및 업데이트 스레드 지연이 발생합니다.
        if (room != nullptr)
        {
            float slowRadius = 10.0f;
            auto monstersInRange = room->GetMonstersInRange(GetX(), GetY(), slowRadius);

            std::set<int32_t> currentInRangeIds;
            Protocol::S_MoveObjectBatch moveBatch;
            moveBatch.set_server_tick(room->GetServerTick());

            // 1. 새로 오라 범위에 들어온 몬스터 처리
            for (auto &m : monstersInRange)
            {
                int32_t mId = m->GetId();
                currentInRangeIds.insert(mId);

                if (_slowedMonsterIds.find(mId) == _slowedMonsterIds.end())
                {
                    m->AddLevelUpSlow(room->GetTotalRunTime(), 999.0f);
                    _slowedMonsterIds.insert(mId);

                    // 배칠 패킷에 추가
                    auto *move = moveBatch.add_moves();
                    move->set_object_id(mId);
                    move->set_x(m->GetX());
                    move->set_y(m->GetY());
                    move->set_vx(m->GetVX());
                    move->set_vy(m->GetVY());
                }
            }

            // 2. 오라 범위를 벗어난 몬스터 처리
            for (auto it = _slowedMonsterIds.begin(); it != _slowedMonsterIds.end();)
            {
                int32_t id = *it;
                if (currentInRangeIds.find(id) == currentInRangeIds.end())
                {
                    auto obj = room->GetObjectManager().GetObject(id);
                    if (obj && obj->GetType() == Protocol::ObjectType::MONSTER)
                    {
                        auto m = std::static_pointer_cast<Monster>(obj);
                        m->RemoveLevelUpSlow();

                        // 배치 패킷에 추가
                        auto *pMove = moveBatch.add_moves();
                        pMove->set_object_id(id);
                        pMove->set_x(m->GetX());
                        pMove->set_y(m->GetY());
                        pMove->set_vx(m->GetVX());
                        pMove->set_vy(m->GetVY());
                    }
                    it = _slowedMonsterIds.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // 변동 사항이 있을 때만 단 한 번 브로드캐스트
            if (moveBatch.moves_size() > 0)
            {
                room->BroadcastPacket(S_MoveObjectBatchPacket(std::move(moveBatch)));
            }

            // [이동 동기화 수정보완] 서버측 레벨업 타임아웃 강제 해제 (안전장치)
            // 클라이언트 패킷 유실이나 크래시 등으로 인해 32초 이상 머물면 강제로 해제합니다.
            float elapsed = room->GetTotalRunTime() - _levelUpStartedAt;
            if (elapsed > (GameConfig::LEVEL_UP_TIMEOUT_SEC + 2.0f))
            {
                LOG_WARN("[Player] Level-up timeout forced exit for player {}", GetId());
                ExitLevelUpState(room);
            }
        }
    }

    // [이동 동기화 수정보완] 레벨업 상태여도 월드는 흐르므로 무기 등의 로직은 계속 업데이트되어야 합니다.
    // 기존에 실수로 들어간 return;을 제거하여 아래 에미터 업데이트가 정상 동작하게 합니다.

    // Don't update logic (emitters) until player is ready in the room
    if (!IsReady())
        return;

    // Update Emitters
    // [Crash Fix] Use index-based iteration to prevent iterator invalidation if _emitters is modified
    for (size_t i = 0; i < _emitters.size(); ++i)
    {
        if (i < _emitters.size() && _emitters[i])
        {
            _emitters[i]->Update(dt, room);
        }
    }

    // Remove expired emitters (e.g. temporary buffs)
    _emitters.erase(
        std::remove_if(
            _emitters.begin(),
            _emitters.end(),
            [](const auto &e)
            {
                return e->IsExpired();
            }
        ),
        _emitters.end()
    );
}

void Player::AddDefaultSkills(const std::vector<int32_t> &skillIds)
{
    if (_inventory == nullptr)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }

    for (int32_t id : skillIds)
    {
        // Add to inventory as level 1 weapon
        _inventory->AddOrUpgradeWeapon(id);
    }

    // Refresh emitters based on inventory
    RefreshInventoryEffects();
}

size_t Player::GetEmitterCount() const
{
    return _emitters.size();
}

void Player::AddEmitter(std::shared_ptr<DamageEmitter> emitter)
{
    _emitters.push_back(std::move(emitter));
}

void Player::ClearEmitters()
{
    _emitters.clear();
}

bool Player::IsInvincible(float currentTime) const
{
    return currentTime < _invincibleUntil;
}

void Player::SetInvincible(float untilTime)
{
    _invincibleUntil = untilTime;
}

void Player::EnterLevelUpState(Room *room)
{
    _isLevelingUp = true;
    _levelUpStartedAt = room ? room->GetTotalRunTime() : 0.0f;
    _invincibleUntil = _levelUpStartedAt + 999.0f; // 레벨업 중 무적 설정

    SetVelocity(0, 0);
    SetState(Protocol::IDLE);
    LOG_DEBUG("[Player] Player {} entered level-up state", GetId());

    if (room != nullptr)
    {
        // Knockback only once for nearby monsters (was 18m, now 30m aura)
        float knockbackRadius = 15.0f; // Initial push is smaller than the slow aura
        auto monsters = room->GetMonstersInRange(GetX(), GetY(), knockbackRadius);
        for (auto &m : monsters)
        {
            // 1. Push Back (Knockback)
            Vector2 dir(m->GetX() - GetX(), m->GetY() - GetY());
            if (dir.IsZero())
                dir = Vector2(1, 0); // Fallback
            dir.Normalize();

            // Moderate push away (Tuned: 10.0f)
            float knockbackForce = 10.0f;
            float knockbackDuration = 0.3f;

            m->SetVelocity(dir.x * knockbackForce, dir.y * knockbackForce);
            m->SetState(Protocol::ObjectState::KNOCKBACK, room->GetTotalRunTime() + knockbackDuration);

            // Broadcast S_Knockback to ensure Client Sync
            Protocol::S_Knockback kbMsg;
            kbMsg.set_object_id(m->GetId());
            kbMsg.set_dir_x(dir.x);
            kbMsg.set_dir_y(dir.y);
            kbMsg.set_force(knockbackForce);
            kbMsg.set_duration(knockbackDuration);

            S_KnockbackPacket kbPkt(std::move(kbMsg));
            room->BroadcastPacket(kbPkt);
        }

        // 2. Initial Slow (Handled by Update aura, but we skip knockbacked ones here to let them fly)
        // Update() will catch everyone within 30m soon.
    }
}

void Player::ExitLevelUpState(Room *room)
{
    _isLevelingUp = false;
    LOG_DEBUG("[Player] Player {} exited level-up state", GetId());

    if (room != nullptr && !_slowedMonsterIds.empty())
    {
        // 추적 중이던 몬스터들의 속도를 복구합니다.
        for (int32_t id : _slowedMonsterIds)
        {
            auto obj = room->GetObjectManager().GetObject(id);
            if (obj != nullptr && obj->GetType() == Protocol::ObjectType::MONSTER)
            {
                auto m = std::static_pointer_cast<Monster>(obj);
                m->RemoveLevelUpSlow();
            }
        }
        LOG_INFO("[Player] Removed LevelUp Slow from {} monsters", _slowedMonsterIds.size());
    }
    _slowedMonsterIds.clear();

    // [이동 동기화 수정보완] 레벨업 종료 시 무적 상태를 즉시 해제합니다.
    _invincibleUntil = 0.0f;

    // 패시브 효과 갱신 (Max HP 상승 등 반영)
    RefreshInventoryEffects();
}

float Player::GetDamageMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "damage")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    return mult;
}

float Player::GetMaxHpMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "max_hp")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    return mult;
}

float Player::GetMovementSpeedMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "speed")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    return mult;
}

float Player::GetCooldownMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "cooldown")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult -= tmpl->levels[level - 1].bonus;
            }
        }
    }
    return std::max(0.1f, mult);
}

float Player::GetAreaMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "area")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    return mult;
}

float Player::GetDurationMultiplier() const
{
    float mult = 1.0f;
    if (_inventory == nullptr)
        return mult;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "duration")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    return mult;
}

int32_t Player::GetAdditionalProjectileCount() const
{
    int32_t count = 0;
    if (_inventory == nullptr)
        return count;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "projectile_count")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                count += static_cast<int32_t>(tmpl->levels[level - 1].bonus);
            }
        }
    }
    return count;
}

int32_t Player::GetAdditionalPierceCount() const
{
    int32_t count = 0;
    if (_inventory == nullptr)
        return count;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "pierce")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                count += static_cast<int32_t>(tmpl->levels[level - 1].bonus);
            }
        }
    }
    return count;
}

PlayerInventory &Player::GetInventory()
{
    if (_inventory == nullptr)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }
    return *_inventory;
}

const std::vector<LevelUpOption> &Player::GetPendingLevelUpOptions() const
{
    return _pendingLevelUpOptions;
}

void Player::SetPendingLevelUpOptions(const std::vector<LevelUpOption> &options)
{
    _pendingLevelUpOptions = options;
}

void Player::ClearPendingLevelUpOptions()
{
    _pendingLevelUpOptions.clear();
}

void Player::RefreshInventoryEffects(Room *room)
{
    if (_inventory == nullptr)
        return;

    // 1. Update Stats based on passives
    int32_t oldMaxHp = _maxHp;
    _maxHp = static_cast<int32_t>(static_cast<double>(_baseMaxHp) * static_cast<double>(GetMaxHpMultiplier()));

    if (_maxHp > oldMaxHp && oldMaxHp > 0)
    {
        // When Max HP increases, increase current HP by the same amount (or proportionally)
        // Here we just add the difference.
        _hp += (_maxHp - oldMaxHp);
    }
    else if (_hp > _maxHp)
    {
        _hp = _maxHp;
    }

    // 2. Refresh weapon emitters
    const auto &weaponIds = _inventory->GetOwnedWeaponIds();
    for (int weaponId : weaponIds)
    {
        int level = _inventory->GetWeaponLevel(weaponId);

        // Find existing emitter for this weapon
        auto it = std::find_if(
            _emitters.begin(),
            _emitters.end(),
            [weaponId](const std::shared_ptr<DamageEmitter> &e)
            {
                return e->GetWeaponId() == weaponId;
            }
        );

        if (it != _emitters.end())
        {
            // Already exists, just update level
            (*it)->SetLevel(level);
        }
        else
        {
            // New weapon, create emitter
            const auto *tmpl = DataManager::Instance().GetWeaponTemplate(weaponId);
            if (tmpl && level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                int skillId = tmpl->levels[level - 1].skillId;
                auto emitter = std::make_shared<DamageEmitter>(
                    skillId, std::static_pointer_cast<Player>(shared_from_this()), weaponId, level
                );
                _emitters.push_back(emitter);
                LOG_INFO("[Player] Added new emitter for weapon {} level {}", weaponId, level);
            }
        }
    }

    SyncInventory(room);
}

void Player::SetGodMode(bool enable)
{
    _godMode = enable;
    LOG_INFO("[Player] God Mode for player {} set to {}", GetId(), enable);
}

bool Player::IsGodMode() const
{
    return _godMode;
}

float Player::GetCriticalChance() const
{
    float chance = 0.05f; // Base 5% critical chance

    // [Future] Add passive bonuses here
    if (_inventory == nullptr)
        return chance;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "critical_chance")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                chance += tmpl->levels[level - 1].bonus;
            }
        }
    }

    return chance;
}

float Player::GetCriticalDamageMultiplier() const
{
    float multiplier = 1.5f; // Base 150% critical damage

    // [Future] Add passive bonuses here
    if (_inventory == nullptr)
        return multiplier;

    for (int id : _inventory->GetOwnedPassiveIds())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveTemplate(id);
        if (tmpl && tmpl->statType == "critical_damage")
        {
            int level = _inventory->GetPassiveLevel(id);
            if (level > 0 && level <= static_cast<int>(tmpl->levels.size()))
            {
                multiplier += tmpl->levels[level - 1].bonus;
            }
        }
    }

    return multiplier;
}

void Player::SyncInventory(Room *room)
{
    if (room == nullptr || _inventory == nullptr)
        return;

    Protocol::S_UpdateInventory msg;

    // 보유 중인 무기 정보 수집
    for (int id : _inventory->GetOwnedWeaponIds())
    {
        auto *item = msg.add_items();
        item->set_id(id);
        item->set_level(_inventory->GetWeaponLevel(id));
        item->set_is_passive(false);
    }

    // 보유 중인 패시브 정보 수집
    for (int id : _inventory->GetOwnedPassiveIds())
    {
        auto *item = msg.add_items();
        item->set_id(id);
        item->set_level(_inventory->GetPassiveLevel(id));
        item->set_is_passive(true);
    }

    // 소유자(해당 플레이어)에게만 개별 전송
    S_UpdateInventoryPacket pkt(msg);
    room->SendToPlayer(_sessionId, pkt);

    LOG_INFO("[Player] 인벤토리 동기화 완료: 플레이어 {} (아이템 {}개)", _id, msg.items_size());
}

} // namespace SimpleGame
