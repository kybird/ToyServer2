#pragma once
#include "System/ToyServerSystem.h"
#include <functional>
#include <utility>
#include <vector>
#include <format>

namespace SimpleGame {

class UserDB
{
public:
    UserDB(std::shared_ptr<System::IDatabase> db) : _db(db)
    {
    }

    void GetUserPoints(int userId, std::function<void(int)> callback)
    {
        std::string sql = std::format("SELECT points FROM user_game_data WHERE user_id = {};", userId);
        _db->AsyncQuery(
            sql,
            [callback](System::DbResult<std::unique_ptr<System::IResultSet>> res)
            {
                if (res.status.IsOk() && res.value.has_value())
                {
                    auto rs = std::move(*res.value);
                    if (rs->Next())
                    {
                        callback(rs->GetInt(0));
                        return;
                    }
                }
                callback(0);
            }
        );
    }

    void AddUserPoints(int userId, int amount, std::function<void(bool)> callback = nullptr)
    {
        std::string sql = std::format(
            "INSERT INTO user_game_data (user_id, points) VALUES ({}, {}) "
            "ON CONFLICT(user_id) DO UPDATE SET points = points + {};",
            userId,
            amount,
            amount
        );

        _db->AsyncExecute(
            sql,
            [userId, callback](System::DbStatus status)
            {
                if (!status.IsOk())
                {
                    LOG_ERROR("Failed to Add Points for User {}: {}", userId, status.message);
                    if (callback)
                        callback(false);
                }
                else
                {
                    if (callback)
                        callback(true);
                }
            }
        );
    }

    void GetUserSkills(int userId, std::function<void(std::vector<std::pair<int, int>>)> callback)
    {
        std::string sql = std::format("SELECT skill_id, level FROM user_skills WHERE user_id = {};", userId);
        _db->AsyncQuery(
            sql,
            [callback](System::DbResult<std::unique_ptr<System::IResultSet>> res)
            {
                std::vector<std::pair<int, int>> skills;
                if (res.status.IsOk() && res.value.has_value())
                {
                    auto rs = std::move(*res.value);
                    while (rs->Next())
                    {
                        skills.emplace_back(rs->GetInt(0), rs->GetInt(1));
                    }
                }
                callback(skills);
            }
        );
    }

    void UnlockSkill(int userId, int skillId, int cost, std::function<void(bool)> callback)
    {
        _db->AsyncRunInTransaction(
            [this, userId, skillId, cost](System::IDatabase *db) -> bool
            {
                // Synchronous Logic running on Worker Thread

                // 1. Check Points
                int points = GetUserPointsSync(db, userId);
                if (points < cost)
                    return false;

                // 2. Transaction
                auto txRes = db->BeginTransaction();
                if (!txRes.status.IsOk() || !txRes.value.has_value())
                    return false;

                auto tx = std::move(*txRes.value);

                // Update Points
                if (!db->Execute(
                           std::format("UPDATE user_game_data SET points = points - {} WHERE user_id={};", cost, userId)
                    )
                         .IsOk())
                    return false;

                // Unlock/Level up Skill
                if (!db->Execute(
                           std::format(
                               "INSERT INTO user_skills (user_id, skill_id, level) VALUES ({}, {}, 1) "
                               "ON CONFLICT(user_id, skill_id) DO UPDATE SET level = level + 1;",
                               userId,
                               skillId
                           )
                    )
                         .IsOk())
                    return false;

                return tx->Commit().IsOk();
            },
            callback
        );
    }

private:
    int GetUserPointsSync(System::IDatabase *db, int userId)
    {
        std::string sql = std::format("SELECT points FROM user_game_data WHERE user_id = {};", userId);
        auto res = db->Query(sql);
        if (res.status.IsOk() && res.value.has_value())
        {
            auto rs = std::move(*res.value);
            if (rs->Next())
            {
                return rs->GetInt(0);
            }
        }
        return 0;
    }

private:
    std::shared_ptr<System::IDatabase> _db;
};

} // namespace SimpleGame
