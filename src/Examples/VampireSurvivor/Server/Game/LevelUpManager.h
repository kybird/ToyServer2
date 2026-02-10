#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace SimpleGame {

class Player;
class Room;

enum class LevelUpOptionType { WEAPON, PASSIVE };

struct LevelUpOption
{
    int32_t optionId;       // 고유 ID (인덱스용)
    LevelUpOptionType type; // 무기 or 패시브
    int32_t itemId;         // 무기 ID or 패시브 ID
    std::string name;
    std::string desc;
    bool isNew; // true = 새 획득, false = 업그레이드
};

/**
 * @brief 레벨업 선택지 생성 및 적용을 담당하는 매니저
 */
class LevelUpManager
{
public:
    LevelUpManager() = default;

    /**
     * @brief 레벨업 선택지 3개 생성
     * @param player 대상 플레이어
     * @return 선택지 목록 (3개)
     */
    std::vector<LevelUpOption> GenerateOptions(Player *player);

    /**
     * @brief 선택 적용
     * @param player 대상 플레이어
     * @param optionIndex 선택한 인덱스 (0, 1, 2)
     * @param room 룸 (DamageEmitter 갱신용)
     */
    void ApplySelection(Player *player, int optionIndex, Room *room);

private:
    /**
     * @brief 선택 가능한 후보 풀 생성
     * @return 모든 가능한 선택지
     */
    static std::vector<LevelUpOption> BuildCandidatePool(Player *player);

    /**
     * @brief 후보 풀에서 랜덤으로 N개 선발
     */
    static std::vector<LevelUpOption> SelectRandom(const std::vector<LevelUpOption> &pool, int count);
};

} // namespace SimpleGame
