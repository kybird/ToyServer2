#include "Entity/PlayerInventory.h"
#include "Core/DataManager.h"
#include "System/ILog.h"

namespace SimpleGame {

bool PlayerInventory::AddOrUpgradeWeapon(int weaponId)
{
    auto it = _weapons.find(weaponId);

    // 이미 보유 중 - 업그레이드
    if (it != _weapons.end())
    {
        const auto *tmpl = DataManager::Instance().GetWeaponInfo(weaponId);
        if (!tmpl)
        {
            LOG_ERROR("[PlayerInventory] Weapon template {} not found", weaponId);
            return false;
        }

        if (it->second >= tmpl->maxLevel)
        {
            LOG_WARN("[PlayerInventory] Weapon {} already at max level {}", weaponId, tmpl->maxLevel);
            return false;
        }

        it->second++;
        LOG_INFO("[PlayerInventory] Upgraded weapon {} to level {}", weaponId, it->second);
        return true;
    }

    // 새로 획득
    if (_weapons.size() >= MAX_WEAPON_SLOTS)
    {
        LOG_WARN("[PlayerInventory] All weapon slots full ({}/{})", _weapons.size(), MAX_WEAPON_SLOTS);
        return false;
    }

    _weapons[weaponId] = 1;
    LOG_INFO("[PlayerInventory] Acquired new weapon {} at level 1", weaponId);
    return true;
}

bool PlayerInventory::AddOrUpgradePassive(int passiveId)
{
    auto it = _passives.find(passiveId);

    // 이미 보유 중 - 업그레이드
    if (it != _passives.end())
    {
        const auto *tmpl = DataManager::Instance().GetPassiveInfo(passiveId);
        if (!tmpl)
        {
            LOG_ERROR("[PlayerInventory] Passive template {} not found", passiveId);
            return false;
        }

        if (it->second >= tmpl->maxLevel)
        {
            LOG_WARN("[PlayerInventory] Passive {} already at max level {}", passiveId, tmpl->maxLevel);
            return false;
        }

        it->second++;
        LOG_INFO("[PlayerInventory] Upgraded passive {} to level {}", passiveId, it->second);
        return true;
    }

    // 새로 획득
    if (_passives.size() >= MAX_PASSIVE_SLOTS)
    {
        LOG_WARN("[PlayerInventory] All passive slots full ({}/{})", _passives.size(), MAX_PASSIVE_SLOTS);
        return false;
    }

    _passives[passiveId] = 1;
    LOG_INFO("[PlayerInventory] Acquired new passive {} at level 1", passiveId);
    return true;
}

std::vector<int> PlayerInventory::GetOwnedWeaponIds() const
{
    std::vector<int> result;
    for (const auto &pair : _weapons)
    {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<int> PlayerInventory::GetOwnedPassiveIds() const
{
    std::vector<int> result;
    for (const auto &pair : _passives)
    {
        result.push_back(pair.first);
    }
    return result;
}

int PlayerInventory::GetWeaponLevel(int weaponId) const
{
    auto it = _weapons.find(weaponId);
    return (it != _weapons.end()) ? it->second : 0;
}

int PlayerInventory::GetPassiveLevel(int passiveId) const
{
    auto it = _passives.find(passiveId);
    return (it != _passives.end()) ? it->second : 0;
}

bool PlayerInventory::HasEmptyWeaponSlot() const
{
    return _weapons.size() < MAX_WEAPON_SLOTS;
}

bool PlayerInventory::HasEmptyPassiveSlot() const
{
    return _passives.size() < MAX_PASSIVE_SLOTS;
}

bool PlayerInventory::IsWeaponMaxLevel(int weaponId) const
{
    auto it = _weapons.find(weaponId);
    if (it == _weapons.end())
        return false;

    const auto *tmpl = DataManager::Instance().GetWeaponInfo(weaponId);
    if (!tmpl)
        return false;

    return it->second >= tmpl->maxLevel;
}

bool PlayerInventory::IsPassiveMaxLevel(int passiveId) const
{
    auto it = _passives.find(passiveId);
    if (it == _passives.end())
        return false;

    const auto *tmpl = DataManager::Instance().GetPassiveInfo(passiveId);
    if (!tmpl)
        return false;

    return it->second >= tmpl->maxLevel;
}

} // namespace SimpleGame
