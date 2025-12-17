#pragma once
#include "System/Database/DBConnectionPool.h"
#include "System/ILog.h"
#include <vector>
#include <utility>

namespace SimpleGame {

using namespace System;

class UserDB
{
public:
    UserDB(std::shared_ptr<DBConnectionPool> dbPool) : _dbPool(dbPool)
    {
    }

    int GetUserPoints(int userId)
    {
        auto conn = _dbPool->Acquire();
        if (!conn) return 0;
        
        int points = 0;
        std::string sql = std::format("SELECT points FROM user_game_data WHERE user_id = {};", userId);
        auto rs = conn->Query(sql);
        if (rs && rs->Next())
        {
            points = rs->GetInt(0);
        }
        _dbPool->Release(conn);
        return points;
    }

    void AddUserPoints(int userId, int amount)
    {
        auto conn = _dbPool->Acquire();
        if (!conn) return;

        // Upsert logic (SQLite: INSERT OR REPLACE or ON CONFLICT)
        // Simple: Try Insert, then Update, or easier: 
        // INSERT INTO user_game_data (user_id, points) VALUES (1, 100) ON CONFLICT(user_id) DO UPDATE SET points = points + 100;
        std::string sql = std::format("INSERT INTO user_game_data (user_id, points) VALUES ({}, {}) "
                                      "ON CONFLICT(user_id) DO UPDATE SET points = points + {};", 
                                      userId, amount, amount);
        
        if (!conn->Execute(sql))
        {
            LOG_ERROR("Failed to Add Points for User {}", userId);
        }
        _dbPool->Release(conn);
    }

    std::vector<std::pair<int, int>> GetUserSkills(int userId)
    {
        std::vector<std::pair<int, int>> skills;
        auto conn = _dbPool->Acquire();
        if (!conn) return skills;

        std::string sql = std::format("SELECT skill_id, level FROM user_skills WHERE user_id = {};", userId);
        auto rs = conn->Query(sql);
        if (rs)
        {
            while (rs->Next())
            {
                skills.emplace_back(rs->GetInt(0), rs->GetInt(1));
            }
        }
        _dbPool->Release(conn);
        return skills;
    }

    bool UnlockSkill(int userId, int skillId, int cost)
    {
        // Transaction needed?
        // 1. Check Points
        // 2. Deduct Points
        // 3. Unlock Skill
        
        auto conn = _dbPool->Acquire();
        if (!conn) return false;

        // Check Points
        int points = 0;
        auto rs = conn->Query(std::format("SELECT points FROM user_game_data WHERE user_id={};", userId));
        if (rs && rs->Next()) points = rs->GetInt(0);
        else 
        {
            _dbPool->Release(conn);
            return false; // User not found
        }

        if (points < cost)
        {
            _dbPool->Release(conn);
            return false;
        }

        // Deduct & Unlock (Use Transaction in real code, here sequential Execute)
        // Or single Transaction BEGIN ... COMMIT
        conn->Execute("BEGIN TRANSACTION;");
        
        bool success = true;
        if (!conn->Execute(std::format("UPDATE user_game_data SET points = points - {} WHERE user_id={};", cost, userId))) success = false;
        if (success && !conn->Execute(std::format("INSERT INTO user_skills (user_id, skill_id, level) VALUES ({}, {}, 1) "
                                                  "ON CONFLICT(user_id, skill_id) DO UPDATE SET level = level + 1;", userId, skillId))) success = false;
        
        if (success) conn->Execute("COMMIT;");
        else conn->Execute("ROLLBACK;");

        _dbPool->Release(conn);
        return success;
    }

private:
    std::shared_ptr<DBConnectionPool> _dbPool;
};

} // namespace SimpleGame
