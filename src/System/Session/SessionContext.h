#pragma once

#include <cstdint>

namespace System {

class ISession;
class IPacket;

/**
 * SessionContext - 현재 Dispatcher 틱 내에서만 유효한 세션 접근 객체
 *
 * 설계 원칙:
 * - Move-only: 복사 불가, 이동만 가능 (람다 캡처 시 명시적 이동 필요)
 * - Scope-bound: Dispatcher가 생성하여 핸들러에 전달
 * - Accessor-only: ISession의 제한된 API만 노출
 *
 * 비동기 작업에서는 SessionContext가 아닌 SessionId를 캡처하고,
 * WithSession(id, callback) 패턴으로 새로운 컨텍스트를 획득해야 합니다.
 */
class SessionContext
{
public:
    // Non-copyable
    SessionContext(const SessionContext &) = delete;
    SessionContext &operator=(const SessionContext &) = delete;

    // Move-only (값 전달 지원)
    SessionContext(SessionContext &&other) noexcept;
    SessionContext &operator=(SessionContext &&other) noexcept;

    // Read-only accessors
    [[nodiscard]] uint64_t Id() const;
    [[nodiscard]] bool IsConnected() const;

    // Controlled actions
    void Send(const IPacket &pkt);
    void Close();
    void OnPong(); // Heartbeat support

private:
    // Only DispatcherImpl can create SessionContext
    friend class DispatcherImpl;
    explicit SessionContext(ISession *session);

    ISession *_session;
};

} // namespace System
