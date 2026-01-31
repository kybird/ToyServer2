#pragma once
#include "System/Database/DatabaseRegistry.h"

namespace System::Database {

/**
 * MySQL 드라이버를 레지스트리에 등록합니다.
 */
std::shared_ptr<IDatabase> CreateMySQL(const DatabaseContext &ctx);

} // namespace System::Database

// REGISTER_DATABASE_DRIVER(mysql, System::Database::CreateMySQL);
