#include "Game/ObjectManager.h"
#include "Game/SpatialGrid.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace SimpleGame {

class MockObject : public GameObject
{
public:
    MockObject(int id, float x, float y) : GameObject(id, Protocol::ObjectType::MONSTER)
    {
        SetPos(x, y);
        SetState(Protocol::ObjectState::IDLE);
    }
    void ReturnToPool() override
    {
    }
};

TEST(SpatialGridTest, InsertAndQuery)
{
    ObjectManager objMgr;
    SpatialGrid grid(100.0f);

    auto obj1 = ::System::RefPtr<MockObject>(new MockObject(1, 50.0f, 50.0f));
    auto obj2 = ::System::RefPtr<MockObject>(new MockObject(2, 150.0f, 50.0f));
    auto obj3 = ::System::RefPtr<MockObject>(new MockObject(3, 60.0f, 60.0f));

    objMgr.AddObject(obj1);
    objMgr.AddObject(obj2);
    objMgr.AddObject(obj3);

    grid.Rebuild(objMgr.GetAllObjects());

    std::vector<::System::RefPtr<GameObject>> results;
    grid.Query(50.0f, 50.0f, 20.0f, results, objMgr);

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
    ObjectManager objMgr;
    SpatialGrid grid(100.0f);

    auto obj1 = ::System::RefPtr<MockObject>(new MockObject(1, 10.0f, 10.0f));
    objMgr.AddObject(obj1);
    grid.Rebuild(objMgr.GetAllObjects());

    obj1->SetPos(250.0f, 250.0f);
    grid.Rebuild(objMgr.GetAllObjects());

    std::vector<::System::RefPtr<GameObject>> resOld;
    grid.Query(10.0f, 10.0f, 50.0f, resOld, objMgr);
    EXPECT_EQ(resOld.size(), 0);

    std::vector<::System::RefPtr<GameObject>> resNew;
    grid.Query(250.0f, 250.0f, 50.0f, resNew, objMgr);
    EXPECT_EQ(resNew.size(), 1);
    EXPECT_EQ(resNew[0]->GetId(), 1);
}

TEST(SpatialGridTest, Remove)
{
    ObjectManager objMgr;
    SpatialGrid grid(100.0f);

    auto obj1 = ::System::RefPtr<MockObject>(new MockObject(1, 50.0f, 50.0f));
    objMgr.AddObject(obj1);
    grid.Rebuild(objMgr.GetAllObjects());

    std::vector<::System::RefPtr<GameObject>> res;
    grid.Query(50.0f, 50.0f, 10.0f, res, objMgr);
    EXPECT_EQ(res.size(), 1);

    objMgr.RemoveObject(obj1->GetId());
    grid.Rebuild(objMgr.GetAllObjects());

    std::vector<::System::RefPtr<GameObject>> resEmpty;
    grid.Query(50.0f, 50.0f, 10.0f, resEmpty, objMgr);
    EXPECT_EQ(resEmpty.size(), 0);
}

} // namespace SimpleGame
