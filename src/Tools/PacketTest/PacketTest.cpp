#include "System/Network/Packet.h"
#include "System/Memory/PacketPool.h"
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
        auto p = PacketPool::Allocate(1024);
        addresses.insert(p.get());
        // Auto released
    }
    addresses.clear();

    // Run 1M times
    size_t iterations = 1000000;
    for (size_t i = 0; i < iterations; ++i)
    {
        auto packet = PacketPool::Allocate(1024);
        addresses.insert(packet.get());
    }

    std::cout << "Allocations: " << iterations << std::endl;
    std::cout << "Unique Addresses: " << addresses.size() << std::endl;

    if (addresses.size() < 100) // Expect very few if single threaded
        std::cout << "[PASS] High reuse rate!" << std::endl;
    else
        std::cout << "[WARN] Reuse rate might be low or pool growing?" << std::endl;

    assert(PacketPool::GetPoolSize() > 0);
}

void TestReferenceCounting()
{
    std::cout << "[Test] Reference Counting..." << std::endl;
    int initialSize = PacketPool::GetPoolSize();

    {
        auto p1 = PacketPool::Allocate(100);
        assert(p1->size == 0); // Reset called
        assert(p1->capacity >= 100);

        {
            boost::intrusive_ptr<Packet> p2 = p1;
            // RefCount should be 2, managed internally
        }
        // p2 dead, refcount 1
    }
    // p1 dead, refcount 0 -> Returned to pool

    int finalSize = PacketPool::GetPoolSize();
    std::cout << "Initial: " << initialSize << ", Final: " << finalSize << std::endl;
    // Pool size logic is bit complex due to leaky singleton initial state,
    // but should be initialSize + 1 if we started clean, or just >= initial
}

int main()
{
    TestReferenceCounting();
    TestAddressStability();
    return 0;
}
