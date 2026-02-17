#include "Game/LevelUpManager.h"
#include "Core/DataManager.h"
#include "Entity/Player.h"
#include "Entity/PlayerInventory.h"
#include "Game/Room.h"
#include "System/ILog.h"
#include <algorithm>
#include <random>

namespace SimpleGame {

std::vector<LevelUpOption> LevelUpManager::GenerateOptions(Player *player)
{
    auto pool = BuildCandidatePool(player);

    if (pool.empty())
    {
        LOG_WARN("[LevelUpManager] No available options for player {}", player->GetId());
        return {};
    }

    // 3개 선택 (풀이 3개 미만이면 전부)
    int count = std::min(3, static_cast<int>(pool.size()));
    return SelectRandom(pool, count);
}

void LevelUpManager::ApplySelection(Player *player, int optionIndex, Room *room)
{
    const auto &options = player->GetPendingLevelUpOptions();

    if (optionIndex < 0 || optionIndex >= static_cast<int>(options.size()))
    {
        LOG_ERROR("[LevelUpManager] Invalid option index {} for player {}", optionIndex, player->GetId());
        return;
    }

    const LevelUpOption selected = options[optionIndex];
    auto &inventory = player->GetInventory();

    bool success = false;
    if (selected.type == LevelUpOptionType::WEAPON)
    {
        success = inventory.AddOrUpgradeWeapon(selected.itemId);
    }
    else // PASSIVE
    {
        success = inventory.AddOrUpgradePassive(selected.itemId);
    }

    if (success)
    {
        player->RefreshInventoryEffects(room);
        player->SyncInventory(room);
        LOG_INFO(
            "[LevelUpManager] Player {} successfully applied {} (ID: {}, Type: {})",
            player->GetId(),
            selected.name,
            selected.itemId,
            (selected.type == LevelUpOptionType::WEAPON ? "Weapon" : "Passive")
        );
    }
    else
    {
        LOG_ERROR(
            "[LevelUpManager] Failed to apply selection (ID: {}) for player {}", selected.itemId, player->GetId()
        );
    }

    player->ClearPendingLevelUpOptions();
}

std::vector<LevelUpOption> LevelUpManager::BuildCandidatePool(Player *player)
{
    std::vector<LevelUpOption> pool;
    auto &inventory = player->GetInventory();
    int optionId = 0;

    // 1. 무기 후보 필터링
    const auto &allWeapons = DataManager::Instance().GetAllWeapons();
    for (const auto &[weaponId, tmpl] : allWeapons)
    {
        int currentLevel = inventory.GetWeaponLevel(weaponId);

        // [진화 조건 체크] 무기가 마스터 레벨이고 필요한 패시브가 있는가?
        if (currentLevel >= tmpl.maxLevel && tmpl.evolutionId > 0)
        {
            if (inventory.GetPassiveLevel(tmpl.evolutionPassiveId) > 0)
            {
                // 진화 아이템을 후보로 등록 (ID는 진화된 무기 ID)
                const auto *evoTmpl = DataManager::Instance().GetWeaponInfo(tmpl.evolutionId);
                if (evoTmpl && inventory.GetWeaponLevel(evoTmpl->id) == 0)
                {
                    LevelUpOption opt;
                    opt.optionId = optionId++;
                    opt.type = LevelUpOptionType::WEAPON;
                    opt.itemId = evoTmpl->id;
                    opt.name = evoTmpl->name + " (EVOLVED)";
                    opt.desc = evoTmpl->description;
                    opt.isNew = true;
                    pool.push_back(opt);
                    continue; // 진화는 특별한 경우이므로 일반 레벨업 로직 건너뜀
                }
            }
        }

        if (currentLevel == 0)
        {
            // 신규 무기 도입 가능 여부 체크
            if (inventory.HasEmptyWeaponSlot())
            {
                LevelUpOption opt;
                opt.optionId = optionId++;
                opt.type = LevelUpOptionType::WEAPON;
                opt.itemId = weaponId;
                opt.name = tmpl.name;
                opt.desc = tmpl.description;
                opt.isNew = true;
                pool.push_back(opt);
            }
        }
        else if (currentLevel < tmpl.maxLevel)
        {
            // 레벨업 가능
            LevelUpOption opt;
            opt.optionId = optionId++;
            opt.type = LevelUpOptionType::WEAPON;
            opt.itemId = weaponId;
            opt.name = tmpl.name;
            opt.desc = (currentLevel < static_cast<int>(tmpl.levels.size()))
                           ? tmpl.levels[static_cast<size_t>(currentLevel)].desc
                           : "Level Up";
            opt.isNew = false;
            pool.push_back(opt);
        }
    }

    // 2. 패시브 후보 필터링
    const auto &allPassives = DataManager::Instance().GetAllPassives();
    for (const auto &[passiveId, tmpl] : allPassives)
    {
        int currentLevel = inventory.GetPassiveLevel(passiveId);

        // [Filtering] 의존성 체크 (필요시 추가)

        if (currentLevel == 0)
        {
            if (inventory.HasEmptyPassiveSlot())
            {
                LevelUpOption opt;
                opt.optionId = optionId++;
                opt.type = LevelUpOptionType::PASSIVE;
                opt.itemId = passiveId;
                opt.name = tmpl.name;
                opt.desc = tmpl.description;
                opt.isNew = true;
                pool.push_back(opt);
            }
        }
        else if (currentLevel < tmpl.maxLevel)
        {
            LevelUpOption opt;
            opt.optionId = optionId++;
            opt.type = LevelUpOptionType::PASSIVE;
            opt.itemId = passiveId;
            opt.name = tmpl.name;
            opt.desc = (currentLevel < static_cast<int>(tmpl.levels.size()))
                           ? tmpl.levels[static_cast<size_t>(currentLevel)].desc
                           : "Level Up";
            opt.isNew = false;
            pool.push_back(opt);
        }
    }

    return pool;
}

std::vector<LevelUpOption> LevelUpManager::SelectRandom(const std::vector<LevelUpOption> &pool, int count)
{
    if (pool.empty())
        return {};
    if (pool.size() <= static_cast<size_t>(count))
        return pool;

    std::vector<LevelUpOption> result;
    std::vector<LevelUpOption> tempPool = pool;

    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < count; ++i)
    {
        int totalWeight = 0;
        for (const auto &opt : tempPool)
        {
            // 가중치 가져오기
            if (opt.type == LevelUpOptionType::WEAPON)
            {
                auto tmpl = DataManager::Instance().GetWeaponInfo(opt.itemId);
                totalWeight += tmpl ? tmpl->weight : 100;
            }
            else
            {
                auto tmpl = DataManager::Instance().GetPassiveInfo(opt.itemId);
                totalWeight += tmpl ? tmpl->weight : 100;
            }
        }

        if (totalWeight <= 0)
            break;

        std::uniform_int_distribution<> dis(0, totalWeight - 1);
        int randomValue = dis(gen);

        int cumulativeWeight = 0;
        for (auto it = tempPool.begin(); it != tempPool.end(); ++it)
        {
            int weight = 100;
            if (it->type == LevelUpOptionType::WEAPON)
            {
                auto tmpl = DataManager::Instance().GetWeaponInfo(it->itemId);
                weight = tmpl ? tmpl->weight : 100;
            }
            else
            {
                auto tmpl = DataManager::Instance().GetPassiveInfo(it->itemId);
                weight = tmpl ? tmpl->weight : 100;
            }

            cumulativeWeight += weight;
            if (randomValue < cumulativeWeight)
            {
                result.push_back(*it);
                tempPool.erase(it); // 중복 선발 방지
                break;
            }
        }
    }

    return result;
}

} // namespace SimpleGame
