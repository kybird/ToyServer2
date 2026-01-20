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

Player::Player(int32_t gameId, System::ISession *session)
    : GameObject(gameId, Protocol::ObjectType::PLAYER), _session(session)
{
    _inventory = std::make_unique<PlayerInventory>();
}

Player::Player() : GameObject(0, Protocol::ObjectType::PLAYER), _session(nullptr)
{
    _inventory = std::make_unique<PlayerInventory>();
}

Player::~Player() = default;

void Player::Initialize(int32_t gameId, System::ISession *session, int32_t hp, float speed)
{
    _id = gameId;
    _session = session;
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

    SetVelocity(dir.x * _speed, dir.y * _speed);
    SetState(Protocol::MOVING);
    LOG_DEBUG(
        "[SetInput] Player={} ClientTick={} Dir=({:.2f}, {:.2f}) Vel=({:.2f},{:.2f})",
        GetId(),
        clientTick,
        dir.x,
        dir.y,
        GetVX(),
        GetVY()
    );
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
    _session = nullptr;
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
}

uint64_t Player::GetSessionId() const
{
    return (_session != nullptr) ? _session->GetId() : 0;
}

System::ISession *Player::GetSession() const
{
    return _session;
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

        // TODO: Move this ID to a shared constant/enum
        constexpr int SKILL_ID_MAX_HP_BONUS = 101;
        if (id == SKILL_ID_MAX_HP_BONUS)
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

    if (_session != nullptr && room != nullptr)
    {
        Protocol::S_ExpChange expMsg;
        expMsg.set_current_exp(_exp);
        expMsg.set_max_exp(_maxExp);
        expMsg.set_level(_level);

        S_ExpChangePacket pkt(std::move(expMsg));
        _session->SendPacket(pkt);

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
            }

            S_LevelUpOptionPacket optPkt(std::move(optionMsg));
            _session->SendPacket(optPkt);

            LOG_INFO("[Player] Sent level-up options to player {}", GetId());
        }
    }
}

void Player::Update(float dt, Room *room)
{
    if (_isLevelingUp)
    {
        // Continuous slow field while leveling up (30m Radius)
        if (room != nullptr)
        {
            float slowRadius = 10.0f; // Aligned with client AoE visual indicator
            auto monstersInRange = room->GetMonstersInRange(GetX(), GetY(), slowRadius);

            // Collect IDs for efficient containment check
            std::set<int32_t> currentInRangeIds;
            for (auto &m : monstersInRange)
            {
                currentInRangeIds.insert(m->GetId());
            }

            // 1. Handle monsters entering the aura
            for (auto &m : monstersInRange)
            {
                if (_slowedMonsterIds.find(m->GetId()) == _slowedMonsterIds.end())
                {
                    // Apply slow with duration (will auto-expire if player exits levelup state)
                    float slowDuration = 999.0f; // Very long duration, will be manually removed on exit
                    m->AddLevelUpSlow(room->GetTotalRunTime(), slowDuration);
                    _slowedMonsterIds.insert(m->GetId());

                    // Immediate packet sync for speed change
                    Protocol::S_MoveObjectBatch moveBatch;
                    moveBatch.set_server_tick(room->GetServerTick());
                    auto *move = moveBatch.add_moves();
                    move->set_object_id(m->GetId());
                    move->set_x(m->GetX());
                    move->set_y(m->GetY());
                    move->set_vx(m->GetVX());
                    move->set_vy(m->GetVY());
                    room->BroadcastPacket(S_MoveObjectBatchPacket(std::move(moveBatch)));
                }
            }

            // 2. Handle monsters exiting the aura
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

                        // Immediate packet sync for speed restoration
                        Protocol::S_MoveObjectBatch moveBatch;
                        moveBatch.set_server_tick(room->GetServerTick());
                        auto *pMove = moveBatch.add_moves();
                        pMove->set_object_id(m->GetId());
                        pMove->set_x(m->GetX());
                        pMove->set_y(m->GetY());
                        pMove->set_vx(m->GetVX());
                        pMove->set_vy(m->GetVY());
                        room->BroadcastPacket(S_MoveObjectBatchPacket(std::move(moveBatch)));
                    }
                    it = _slowedMonsterIds.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        return;
    }

    // Don't update logic (emitters) until player is ready in the room
    if (!IsReady())
        return;

    // Update Emitters
    for (auto &emitter : _emitters)
    {
        emitter->Update(dt, room);
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
        // Restore speed for tracked monsters
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
                // Cooldown bonus is usually negative (e.g. -0.1) to reduce cooldown
                mult += tmpl->levels[level - 1].bonus;
            }
        }
    }
    // Clamp to minimum 0.1 to avoid instant fire or divide by zero if used as divider
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

void Player::RefreshInventoryEffects()
{
    if (_inventory == nullptr)
        return;

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

} // namespace SimpleGame
