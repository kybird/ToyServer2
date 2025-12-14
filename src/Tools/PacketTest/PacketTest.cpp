#include "System/Dispatcher/MessagePool.h"
#include "System/Pch.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace System;

void TestAddressStability()
{
    std::cout << "[Test] Address Stability..." << std::endl;
    std::unordered_set<void *> addresses;

    // Warmup
    for (int i = 0; i < 100; ++i)
    {
        auto p = MessagePool::AllocatePacket(1024);
        addresses.insert(p);
        MessagePool::Free(p);
    }
    addresses.clear();

    // Run 1M times
    size_t iterations = 1000000;
    for (size_t i = 0; i < iterations; ++i)
    {
        auto msg = MessagePool::AllocatePacket(1024);
        addresses.insert(msg);
        MessagePool::Free(msg);
    }

    std::cout << "Allocations: " << iterations << std::endl;
    std::cout << "Unique Addresses: " << addresses.size() << std::endl;

    if (addresses.size() < 100) // Expect very few if single threaded
        std::cout << "[PASS] High reuse rate!" << std::endl;
    else
        std::cout << "[WARN] Reuse rate might be low or pool growing?" << std::endl;

    assert(MessagePool::GetPoolSize() >= 0);
}

void TestAllocationDeallocation()
{
    std::cout << "[Test] Allocation/Deallocation..." << std::endl;
    int initialSize = MessagePool::GetPoolSize();

    {
        auto p1 = MessagePool::AllocatePacket(100);
        assert(p1 != nullptr);
        assert(p1->length == 100);

        MessagePool::Free(p1);
    }

    int finalSize = MessagePool::GetPoolSize();
    std::cout << "Initial: " << initialSize << ", Final: " << finalSize << std::endl;
}

int main()
{
    TestAllocationDeallocation();
    TestAddressStability();
    return 0;
}
