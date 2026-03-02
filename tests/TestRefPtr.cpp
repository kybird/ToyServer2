#include "System/Memory/RefCounted.h"
#include "System/Memory/RefPtr.h"
#include <gtest/gtest.h>

namespace System {
namespace Testing {

using namespace System;

// Mock object for testing
class MockEntity : public RefCounted<MockEntity>
{
public:
    int value = 0;
    bool poolReturned = false;

    void ReturnToPool()
    {
        poolReturned = true;
    }
};

class RefPtrTest : public ::testing::Test
{
};

TEST_F(RefPtrTest, DefaultConstructor)
{
    RefPtr<MockEntity> ptr;
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_FALSE(ptr);
}

TEST_F(RefPtrTest, RawPointerConstructor)
{
    auto *raw = new MockEntity();
    EXPECT_EQ(raw->GetRefCount(), 0);

    {
        RefPtr<MockEntity> ptr(raw);
        EXPECT_EQ(ptr->GetRefCount(), 1);
        EXPECT_EQ(ptr.get(), raw);
    }

    EXPECT_TRUE(raw->poolReturned);
    delete raw; // Manual cleanup since ReturnToPool just sets flag
}

TEST_F(RefPtrTest, CopyConstructor)
{
    auto *raw = new MockEntity();
    RefPtr<MockEntity> ptr1(raw);
    EXPECT_EQ(ptr1->GetRefCount(), 1);

    {
        RefPtr<MockEntity> ptr2(ptr1);
        EXPECT_EQ(ptr1->GetRefCount(), 2);
        EXPECT_EQ(ptr2->GetRefCount(), 2);
        EXPECT_EQ(ptr1.get(), ptr2.get());
    }

    EXPECT_EQ(ptr1->GetRefCount(), 1);
    delete raw;
}

TEST_F(RefPtrTest, MoveConstructor)
{
    auto *raw = new MockEntity();
    RefPtr<MockEntity> ptr1(raw);
    EXPECT_EQ(ptr1->GetRefCount(), 1);

    RefPtr<MockEntity> ptr2(std::move(ptr1));
    EXPECT_EQ(ptr2->GetRefCount(), 1);
    EXPECT_EQ(ptr2.get(), raw);
    EXPECT_EQ(ptr1.get(), nullptr);

    delete raw;
}

TEST_F(RefPtrTest, AssignmentOperators)
{
    auto *raw1 = new MockEntity();
    auto *raw2 = new MockEntity();

    RefPtr<MockEntity> ptr1(raw1);
    RefPtr<MockEntity> ptr2(raw2);

    EXPECT_EQ(ptr1->GetRefCount(), 1);
    EXPECT_EQ(ptr2->GetRefCount(), 1);

    // Copy assign
    ptr1 = ptr2;
    EXPECT_EQ(ptr1.get(), raw2);
    EXPECT_EQ(ptr2.get(), raw2);
    EXPECT_EQ(raw2->GetRefCount(), 2);
    EXPECT_TRUE(raw1->poolReturned);

    // Move assign
    auto *raw3 = new MockEntity();
    RefPtr<MockEntity> ptr3(raw3);

    ptr1 = std::move(ptr3);
    EXPECT_EQ(ptr1.get(), raw3);
    EXPECT_EQ(ptr3.get(), nullptr);
    EXPECT_EQ(raw3->GetRefCount(), 1);

    delete raw1;
    delete raw2;
    delete raw3;
}

TEST_F(RefPtrTest, ResetAndDetach)
{
    auto *raw = new MockEntity();
    RefPtr<MockEntity> ptr(raw);

    EXPECT_EQ(ptr->GetRefCount(), 1);

    ptr.reset();
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_TRUE(raw->poolReturned); // Should be returned to pool
    EXPECT_EQ(raw->GetRefCount(), 0);

    RefPtr<MockEntity> ptr2(raw); // Should be ref 1 again
    raw->poolReturned = false;

    MockEntity *detached = ptr2.detach();
    EXPECT_EQ(ptr2.get(), nullptr);
    EXPECT_EQ(detached, raw);
    EXPECT_EQ(detached->GetRefCount(), 1); // Ref count stays 1
    EXPECT_FALSE(raw->poolReturned);       // Not returned when detached

    delete raw;
}

} // namespace Testing
} // namespace System
