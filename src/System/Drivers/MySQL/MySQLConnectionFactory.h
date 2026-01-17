#pragma once

#include "System/Database/IConnectionFactory.h"
#include "System/Drivers/MySQL/MySQLConnection.h"

namespace System {

/**
 * MySQL 커넥션 팩토리
 * - MySQLConfig를 받아 MySQLConnection 생성
 */
class MySQLConnectionFactory : public IConnectionFactory
{
public:
    MySQLConnectionFactory(const MySQLConfig &config) : _config(config)
    {
    }

    IConnection *CreateConnection() override
    {
        return new MySQLConnection(_config);
    }

private:
    MySQLConfig _config;
};

} // namespace System
