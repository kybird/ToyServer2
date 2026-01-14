#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace SimpleGame {

/**
 * @brief 플레이어의 무기/패시브 슬롯을 관리하는 클래스
 */
class PlayerInventory
{
public:
    static constexpr int MAX_WEAPON_SLOTS = 6;
    static constexpr int MAX_PASSIVE_SLOTS = 6;
    static constexpr int MAX_WEAPON_LEVEL = 8;
    static constexpr int MAX_PASSIVE_LEVEL = 5;

    PlayerInventory() = default;

    /**
     * @brief 무기 추가 또는 업그레이드
     * @return true if successful (slot available or upgraded), false if all slots full and not owned
     */
    bool AddOrUpgradeWeapon(int weaponId);

    /**
     * @brief 패시브 추가 또는 업그레이드
     * @return true if successful (slot available or upgraded), false if all slots full and not owned
     */
    bool AddOrUpgradePassive(int passiveId);

    /**
     * @brief 소유한 무기 ID 목록 반환
     */
    std::vector<int> GetOwnedWeaponIds() const;

    /**
     * @brief 소유한 패시브 ID 목록 반환
     */
    std::vector<int> GetOwnedPassiveIds() const;

    /**
     * @brief 무기 레벨 조회
     * @return 0 if not owned
     */
    int GetWeaponLevel(int weaponId) const;

    /**
     * @brief 패시브 레벨 조회
     * @return 0 if not owned
     */
    int GetPassiveLevel(int passiveId) const;

    /**
     * @brief 빈 무기 슬롯이 있는지 확인
     */
    bool HasEmptyWeaponSlot() const;

    /**
     * @brief 빈 패시브 슬롯이 있는지 확인
     */
    bool HasEmptyPassiveSlot() const;

    /**
     * @brief 무기가 최대 레벨인지 확인
     */
    bool IsWeaponMaxLevel(int weaponId) const;

    /**
     * @brief 패시브가 최대 레벨인지 확인
     */
    bool IsPassiveMaxLevel(int passiveId) const;

private:
    std::unordered_map<int, int> _weapons;  // weaponId -> level
    std::unordered_map<int, int> _passives; // passiveId -> level
};

} // namespace SimpleGame
