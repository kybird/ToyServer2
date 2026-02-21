#pragma once

namespace SimpleGame {

class Monster;
class Room;

/**
 * @brief 몬스터 이동 로직을 위한 인터페이스입니다.
 *
 * 핵심 AI 클래스를 수정하지 않고도 다양한 조향 동작(분리, 군집, 길찾기 등)을
 * 교체하여 사용할 수 있도록 합니다.
 */
class IMovementStrategy
{
public:
    virtual ~IMovementStrategy() = default;

    /**
     * @brief 몬스터의 다음 속도를 계산합니다.
     *
     * @param monster 몬스터 엔티티입니다.
     * @param room 방 객체입니다 (공간 쿼리용).
     * @param dt 델타 타임입니다.
     * @param targetX 목표 지점(예: 플레이어)의 X 좌표입니다.
     * @param targetY 목표 지점의 Y 좌표입니다.
     * @param outVx 계산된 X 속도를 반환할 변수입니다.
     * @param outVy 계산된 Y 속도를 반환할 변수입니다.
     */
    virtual void CalculateMovement(
        Monster *monster, Room *room, float dt, float targetX, float targetY, float &outVx, float &outVy
    ) = 0;
};

} // namespace SimpleGame
