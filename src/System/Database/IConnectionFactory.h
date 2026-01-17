#pragma once

#include "System/Database/IConnection.h"
#include <memory>

namespace System {

/**
 * DB 커넥션 생성 팩토리 인터페이스
 * - DatabaseImpl 외부에서 구현하여 주입
 */
struct IConnectionFactory
{
    virtual ~IConnectionFactory() = default;

    /**
     * 새로운 커넥션 인스턴스 생성
     * - 연결이 맺어지지 않은 상태의 인스턴스를 반환할 수도 있고,
     *   Connect()가 호출된 상태를 반환할 수도 있음 (구현 나름이나 보통은 Connect 전)
     * - DatabaseImpl은 이 객체를 받은 후 Connect()를 호출하여 사용함.
     */
    virtual IConnection *CreateConnection() = 0;
};

} // namespace System
