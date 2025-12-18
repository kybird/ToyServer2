#pragma once

#include "System/IDatabase.h"
#include <memory>
#include <string>

namespace System {

/**
 * DB 드라이버 내부 커넥션 인터페이스
 * - DatabaseImpl(Pool)에서 관리됨.
 * - 단일 스레드 사용 전제.
 */
class IConnection
{
public:
    virtual ~IConnection() = default;

    virtual bool Connect(const std::string &connStr) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() = 0;
    virtual bool Ping() = 0;

    // 작업 수행
    virtual DbStatus Execute(const std::string &sql) = 0;
    virtual DbResult<std::unique_ptr<IResultSet>> Query(const std::string &sql) = 0;
    virtual DbResult<std::unique_ptr<IPreparedStatement>> Prepare(const std::string &sql) = 0;

    // 트랜잭션 제어
    virtual DbStatus BeginTransaction() = 0;
    virtual DbStatus Commit() = 0;
    virtual DbStatus Rollback() = 0;

    /**
     * 커넥션 상태 초기화 (풀 반환 전 호출)
     * - 미종료 트랜잭션 롤백, 세션 변수 초기화 등
     */
    virtual void ResetState() = 0;

    /**
     * 드라이버 특성 확인
     */
    virtual bool SupportsPreparedStatements() const = 0;
    virtual bool SupportsTransactions() const = 0;
};

} // namespace System
