#include "Entity/Player.h"
#include "Core/DataManager.h"
#include "Entity/PlayerInventory.h"
#include "Game/DamageEmitter.h"
#include "Game/GameConfig.h"
#include "Game/LevelUpManager.h"
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

    // Initialize inventory
    if (!_inventory)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }
    _pendingLevelUpOptions.clear();
}

void Player::ApplyInput(uint32_t clientTick, int32_t dx, int32_t dy)
{
    _lastInputTick = clientTick;

    if (IsControlDisabled())
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

    // 무적 상태 체크
    if (IsInvincible(room->GetTotalRunTime()))
        return;

    // 무적 시간 적용
    SetInvincible(room->GetTotalRunTime() + GameConfig::PLAYER_INVINCIBLE_DURATION);

    _hp -= damage;
    if (_hp <= 0)
    {
        _hp = 0;
        SetState(Protocol::ObjectState::DEAD);
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
    _isReady = false;

    // Reset inventory
    if (_inventory)
    {
        _inventory = std::make_unique<PlayerInventory>();
    }
    _pendingLevelUpOptions.clear();
}

uint64_t Player::GetSessionId() const
{
    return _session ? _session->GetId() : 0;
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

        if (id == 101)
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
    _exp += amount;

    // Level up check
    bool leveledUp = false;
    while (_exp >= _maxExp)
    {
        _exp -= _maxExp;
        _level++;
        _maxExp = 100 + (_level - 1) * 50; // Increase required exp per level
        LOG_INFO("Player {} leveled up to {}!", GetId(), _level);
        leveledUp = true;
    }

    // Send exp change to client
    if (_session && room)
    {
        Protocol::S_ExpChange expMsg;
        expMsg.set_current_exp(_exp);
        expMsg.set_max_exp(_maxExp);
        expMsg.set_level(_level);

        S_ExpChangePacket pkt(std::move(expMsg));
        _session->SendPacket(pkt);

        // [Temp Hack] Automatically upgrade Projectile Mirror (ID: 6) on Level Up
        if (leveledUp)
        {
            _inventory->AddOrUpgradePassive(6);
            RefreshInventoryEffects();
            LOG_INFO("[Temp] Player {} automatically upgraded Projectile Mirror (ID: 6) via Level Up", GetId());
        }
    }
}

void Player::Update(float dt, Room *room)
{
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
    if (!_inventory)
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

float Player::GetDamageMultiplier() const
{
    float mult = 1.0f;
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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
    if (!_inventory)
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

} // namespace SimpleGame
