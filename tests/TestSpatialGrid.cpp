#include "Entity/GameObject.h"
#include "Game/SpatialGrid.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace SimpleGame;

class MockObject : public GameObject
{
public:
    MockObject(int id, float x, float y) : GameObject(id, Protocol::ObjectType::UNKNOWN)
    {
        SetPos(x, y);
    }
};

TEST(SpatialGridTest, InsertAndQuery)
{
    SpatialGrid grid(100.0f);
    auto obj1 = std::make_shared<MockObject>(1, 50.0f, 50.0f);
    auto obj2 = std::make_shared<MockObject>(2, 150.0f, 50.0f); // Different cell
    auto obj3 = std::make_shared<MockObject>(3, 60.0f, 60.0f);  // Same cell as obj1

    grid.Add(obj1);
    grid.Add(obj2);
    grid.Add(obj3);

    // Query covers obj1 and obj3, but not obj2
    auto results = grid.QueryRange(50.0f, 50.0f, 20.0f);

    // obj1 (dist 0), obj3 (dist ~14.14) -> Both within 20
    EXPECT_EQ(results.size(), 2);

    bool found1 = false;
    bool found3 = false;
    for (auto &obj : results)
    {
        if (obj->GetId() == 1)
            found1 = true;
        if (obj->GetId() == 3)
            found3 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found3);
}

TEST(SpatialGridTest, UpdatePosition)
{
    SpatialGrid grid(100.0f); // Cell 0,0 is [0, 100)
    auto obj1 = std::make_shared<MockObject>(1, 10.0f, 10.0f);
    grid.Add(obj1);

    // Move to 200, 200 (Cell 2,2)
    obj1->SetPos(250.0f, 250.0f);
    grid.Update(obj1);

    // Query old location -> Empty
    auto resOld = grid.QueryRange(10.0f, 10.0f, 50.0f);
    EXPECT_EQ(resOld.size(), 0);

    // Query new location -> Found
    auto resNew = grid.QueryRange(250.0f, 250.0f, 50.0f);
    EXPECT_EQ(resNew.size(), 1);
    EXPECT_EQ(resNew[0]->GetId(), 1);
}

TEST(SpatialGridTest, Remove)
{
    SpatialGrid grid(100.0f);
    auto obj1 = std::make_shared<MockObject>(1, 50.0f, 50.0f);
    grid.Add(obj1);

    auto res = grid.QueryRange(50.0f, 50.0f, 10.0f);
    EXPECT_EQ(res.size(), 1);

    grid.Remove(obj1);
    auto resEmpty = grid.QueryRange(50.0f, 50.0f, 10.0f);
    EXPECT_EQ(resEmpty.size(), 0);
}
