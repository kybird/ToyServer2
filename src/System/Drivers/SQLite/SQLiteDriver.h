#pragma once
#include "System/Database/DatabaseRegistry.h"

namespace System::Database {

/**
 * SQLite 드라이버를 레지스트리에 등록합니다.
 * Main.cpp에서 이 헤더를 include하면 자동으로 등록 매크로가 실행됩니다.
 */
std::shared_ptr<IDatabase> CreateSQLite(const DatabaseContext &ctx);

} // namespace System::Database

// 매크로 제거: 명시적 등록 사용
// REGISTER_DATABASE_DRIVER(sqlite, System::Database::CreateSQLite);
