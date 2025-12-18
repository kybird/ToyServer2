#pragma once
#include "System/ToyServerSystem.h"
#include <utility>
#include <vector>

namespace SimpleGame {

class UserDB
{
public:
    UserDB(std::shared_ptr<System::IDatabase> db) : _db(db)
    {
    }

    int GetUserPoints(int userId)
    {
        std::string sql = std::format("SELECT points FROM user_game_data WHERE user_id = {};", userId);
        auto res = _db->Query(sql);
        if (res.status.IsOk() && res.value.has_value())
        {
            auto rs = std::move(*res.value); // 소유권 이전 (RAII)
            if (rs->Next())
            {
                return rs->GetInt(0);
            }
        }
        return 0;
    }

    void AddUserPoints(int userId, int amount)
    {
        std::string sql = std::format(
            "INSERT INTO user_game_data (user_id, points) VALUES ({}, {}) "
            "ON CONFLICT(user_id) DO UPDATE SET points = points + {};",
            userId,
            amount,
            amount
        );

        auto status = _db->Execute(sql);
        if (!status.IsOk())
        {
            LOG_ERROR("Failed to Add Points for User {}: {}", userId, status.message);
        }
    }

    std::vector<std::pair<int, int>> GetUserSkills(int userId)
    {
        std::vector<std::pair<int, int>> skills;
        std::string sql = std::format("SELECT skill_id, level FROM user_skills WHERE user_id = {};", userId);
        auto res = _db->Query(sql);
        if (res.status.IsOk() && res.value.has_value())
        {
            auto rs = std::move(*res.value); // 소유권 이전 (RAII)
            while (rs->Next())
            {
                skills.emplace_back(rs->GetInt(0), rs->GetInt(1));
            }
        }
        return skills;
    }

    bool UnlockSkill(int userId, int skillId, int cost)
    {
        // 1. Check Points
        int points = GetUserPoints(userId);
        if (points < cost)
            return false;

        // 2. Transaction
        auto txRes = _db->BeginTransaction();
        if (!txRes.status.IsOk() || !txRes.value.has_value())
            return false;

        auto tx = std::move(*txRes.value); // 소유권 이전 (RAII)

        // Update Points
        if (!_db->Execute(std::format("UPDATE user_game_data SET points = points - {} WHERE user_id={};", cost, userId))
                 .IsOk())
            return false;

        // Unlock/Level up Skill
        if (!_db->Execute(
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
    }

private:
    std::shared_ptr<System::IDatabase> _db;
};

} // namespace SimpleGame
