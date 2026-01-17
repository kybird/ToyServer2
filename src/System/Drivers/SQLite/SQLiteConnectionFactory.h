#pragma once

#include "System/Database/IConnectionFactory.h"
#include "System/Drivers/SQLite/SQLiteConnection.h"

namespace System {

class SQLiteConnectionFactory : public IConnectionFactory
{
public:
    IConnection *CreateConnection() override
    {
        return new SQLiteConnection();
    }
};

} // namespace System
