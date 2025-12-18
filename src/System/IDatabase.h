#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace System {

/**
 * 데이터베이스 작업 상태 코드
 */

enum class DbStatusCode {
    DB_OK,
    DB_ERROR,
    DB_TIMEOUT,
    DB_BUSY,
    DB_AUTH_FAIL,
    DB_INVALID_QUERY,
    DB_TRANSACTION_ACTIVE,
    DB_CONNECTION_FAILURE
};

/**
 * 데이터베이스 작업 상태 결과
 */
struct DbStatus
{
    DbStatusCode code;
    std::string message;

    bool IsOk() const
    {
        return code == DbStatusCode::DB_OK;
    }

    static DbStatus Ok()
    {
        return {DbStatusCode::DB_OK, ""};
    }
    static DbStatus Error(const std::string &msg)
    {
        return {DbStatusCode::DB_ERROR, msg};
    }
    static DbStatus Timeout(const std::string &msg = "")
    {
        return {DbStatusCode::DB_TIMEOUT, msg};
    }
};

/**
 * DbResult<T> 사용 규칙:
 * - status.IsOk() == true 인 경우에만 value.has_value() == true 가 보장됨.
 * - status.IsOk() == false 인 경우 value는 반드시 nullopt 임.
 * - [중요] T가 unique_ptr인 경우(ITransaction, IResultSet 등), 반드시 std::move(*value)를 사용하여
 *   로컬 변수로 소유권을 이전해야 한다. (RAII 계약 준수)
 * - 단순 Reference(auto&) 사용 시 RAII 객체의 수명이 결과 객체(DbResult)에 종속되어 설계 의도가 파괴된다.
 */
template <typename T> struct DbResult
{
    DbStatus status;
    std::optional<T> value;

    static DbResult Success(T &&val)
    {
        return {DbStatus::Ok(), std::forward<T>(val)};
    }
    static DbResult Fail(DbStatusCode code, const std::string &msg)
    {
        return {{code, msg}, std::nullopt};
    }
};

/**
 * SQL 쿼리 결과 셋 인터페이스
 * - Move-only
 */
class IResultSet
{
public:
    virtual ~IResultSet() = default;
    IResultSet(const IResultSet &) = delete;
    IResultSet &operator=(const IResultSet &) = delete;
    IResultSet(IResultSet &&) = default;

    virtual bool Next() = 0;
    virtual int GetInt(int idx) = 0;
    virtual std::string GetString(int idx) = 0;
    virtual double GetDouble(int idx) = 0;

protected:
    IResultSet() = default;
};

/**
 * Prepared Statement 인터페이스
 * - Move-only
 * - 커넥션 종속적이며, 커넥션 반환 시 무효화됨.
 */
class IPreparedStatement
{
public:
    virtual ~IPreparedStatement() = default;
    IPreparedStatement(const IPreparedStatement &) = delete;
    IPreparedStatement &operator=(const IPreparedStatement &) = delete;
    IPreparedStatement(IPreparedStatement &&) = default;

    virtual DbStatus BindInt(int idx, int val) = 0;
    virtual DbStatus BindString(int idx, const std::string &val) = 0;
    virtual DbStatus BindDouble(int idx, double val) = 0;

    virtual DbResult<std::unique_ptr<IResultSet>> ExecuteQuery() = 0;
    virtual DbStatus ExecuteUpdate() = 0;

protected:
    IPreparedStatement() = default;
};

/**
 * 트랜잭션 수명 관리 인터페이스 (RAII)
 * - Commit()이 성공적으로 호출된 이후에는 소멸자는 어떠한 DB 동작도 수행하지 않음.
 * - Commit()이 호출되지 않았거나 실패한 상태에서 소멸될 때만 자동 Rollback을 시도함.
 */
class ITransaction
{
public:
    virtual ~ITransaction() = default;
    ITransaction(const ITransaction &) = delete;
    ITransaction &operator=(const ITransaction &) = delete;
    ITransaction(ITransaction &&) = default;

    virtual DbStatus Commit() = 0;

protected:
    ITransaction() = default;
};

/**
 * 데이터베이스 통합 인터페이스 (Facade & Generic Pool)
 * - Thread-safe
 */
class IDatabase
{
public:
    virtual ~IDatabase() = default;

    /**
     * Database 시스템 생성 팩토리
     * @param driverType "sqlite", "mysql" 등
     * @param connStr 연결 문자열
     * @param defaultTimeoutMs 기본 타임아웃 (ms), 기본값 5000
     */
    static std::shared_ptr<IDatabase>
    Create(const std::string &driverType, const std::string &connStr, int poolSize, int defaultTimeoutMs = 5000);

    // 단순 쿼리/실행 (내부에서 커넥션 획득 및 반환 자동 수행)
    virtual DbResult<std::unique_ptr<IResultSet>> Query(const std::string &sql) = 0;
    virtual DbStatus Execute(const std::string &sql) = 0;

    // Prepared Statement 생성
    virtual DbResult<std::unique_ptr<IPreparedStatement>> Prepare(const std::string &sql) = 0;

    // 트랜잭션 시작 (RAII)
    virtual DbResult<std::unique_ptr<ITransaction>> BeginTransaction() = 0;

protected:
    IDatabase() = default;
};

} // namespace System
