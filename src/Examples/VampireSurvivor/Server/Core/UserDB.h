#pragma once
#include "System/ToyServerSystem.h"
#include <format>
#include <functional>
#include <utility>
#include <vector>

namespace SimpleGame {

class UserDB
{
public:
    UserDB(std::shared_ptr<System::IDatabase> db);

    void InitSchema(); // [New]

    void GetUserPoints(int userId, std::function<void(int)> callback);
    void AddUserPoints(int userId, int amount, std::function<void(bool)> callback = nullptr);
    void GetUserSkills(int userId, std::function<void(std::vector<std::pair<int, int>>)> callback);
    void UnlockSkill(int userId, int skillId, int cost, std::function<void(bool)> callback);

private:
    int GetUserPointsSync(System::IDatabase *db, int userId);

private:
    std::shared_ptr<System::IDatabase> _db;
};

} // namespace SimpleGame
