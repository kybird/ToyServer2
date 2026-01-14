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

    const auto &selected = options[optionIndex];
    auto &inventory = player->GetInventory();

    bool success = false;
    if (selected.type == LevelUpOptionType::WEAPON)
    {
        success = inventory.AddOrUpgradeWeapon(selected.itemId);
        if (success)
        {
            // TODO: DamageEmitter 갱신 (Phase 3)
            LOG_INFO("[LevelUpManager] Player {} selected weapon {}", player->GetId(), selected.itemId);
        }
    }
    else // PASSIVE
    {
        success = inventory.AddOrUpgradePassive(selected.itemId);
        if (success)
        {
            // TODO: 패시브 효과 적용 (Phase 3)
            LOG_INFO("[LevelUpManager] Player {} selected passive {}", player->GetId(), selected.itemId);
        }
    }

    if (success)
    {
        player->RefreshInventoryEffects();
        LOG_INFO("[LevelUpManager] Player {} selection applied and effects refreshed", player->GetId());
    }
    else
    {
        LOG_ERROR("[LevelUpManager] Failed to apply selection for player {}", player->GetId());
    }

    // 선택지 초기화
    player->ClearPendingLevelUpOptions();
}

std::vector<LevelUpOption> LevelUpManager::BuildCandidatePool(Player *player)
{
    std::vector<LevelUpOption> pool;
    auto &inventory = player->GetInventory();
    int optionId = 0;

    // 무기 후보
    const auto &allWeapons = DataManager::Instance().GetAllWeapons();
    for (const auto &[weaponId, tmpl] : allWeapons)
    {
        int currentLevel = inventory.GetWeaponLevel(weaponId);

        if (currentLevel == 0)
        {
            // 새로 획득 가능 (빈 슬롯이 있을 때만)
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
            // 업그레이드 가능
            LevelUpOption opt;
            opt.optionId = optionId++;
            opt.type = LevelUpOptionType::WEAPON;
            opt.itemId = weaponId;
            opt.name = tmpl.name;

            // 다음 레벨 설명
            if (currentLevel < static_cast<int>(tmpl.levels.size()))
            {
                opt.desc = tmpl.levels[currentLevel].desc; // 다음 레벨 (0-indexed)
            }
            else
            {
                opt.desc = "Level " + std::to_string(currentLevel + 1);
            }

            opt.isNew = false;
            pool.push_back(opt);
        }
    }

    // 패시브 후보
    const auto &allPassives = DataManager::Instance().GetAllPassives();
    for (const auto &[passiveId, tmpl] : allPassives)
    {
        int currentLevel = inventory.GetPassiveLevel(passiveId);

        if (currentLevel == 0)
        {
            // 새로 획득 가능 (빈 슬롯이 있을 때만)
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
            // 업그레이드 가능
            LevelUpOption opt;
            opt.optionId = optionId++;
            opt.type = LevelUpOptionType::PASSIVE;
            opt.itemId = passiveId;
            opt.name = tmpl.name;

            // 다음 레벨 설명
            if (currentLevel < static_cast<int>(tmpl.levels.size()))
            {
                opt.desc = tmpl.levels[currentLevel].desc; // 다음 레벨 (0-indexed)
            }
            else
            {
                opt.desc = "Level " + std::to_string(currentLevel + 1);
            }

            opt.isNew = false;
            pool.push_back(opt);
        }
    }

    LOG_INFO("[LevelUpManager] Built candidate pool with {} options for player {}", pool.size(), player->GetId());
    return pool;
}

std::vector<LevelUpOption> LevelUpManager::SelectRandom(const std::vector<LevelUpOption> &pool, int count)
{
    if (pool.size() <= static_cast<size_t>(count))
    {
        return pool; // 풀이 작으면 전부 반환
    }

    std::vector<LevelUpOption> result;
    std::vector<int> indices(pool.size());
    for (size_t i = 0; i < pool.size(); ++i)
    {
        indices[i] = static_cast<int>(i);
    }

    // 랜덤 셔플
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    // 앞에서 count개 선택
    for (int i = 0; i < count; ++i)
    {
        result.push_back(pool[indices[i]]);
    }

    return result;
}

} // namespace SimpleGame
