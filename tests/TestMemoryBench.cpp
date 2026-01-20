#include "System/Dispatcher/MessagePool.h"
#include <array>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>


using namespace System;

// 캐시 라인(64B) 오염을 유도하는 워크로드
struct SmallWorkload
{
    std::array<char, 64> padding;
    void operator()()
    {
        const_cast<volatile char &>(padding[0]) = 1;
        const_cast<volatile char &>(padding[63]) = 2;
    }
};

class MemoryStrategyBench : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MessagePool::Prepare(100000);
    }
    void TearDown() override
    {
        MessagePool::Clear();
    }
};

const int OP_COUNT = 100000;
const int THREADS = 4;

// 1. 소형 객체 + 시스템 할당 (Best Case)
TEST_F(MemoryStrategyBench, Small_System_LFH)
{
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> workers;
    for (int t = 0; t < THREADS; ++t)
    {
        workers.emplace_back(
            []()
            {
                for (int i = 0; i < OP_COUNT / THREADS; ++i)
                {
                    LambdaMessage *m = new LambdaMessage();
                    m->task = SmallWorkload();
                    m->task();
                    delete m;
                }
            }
        );
    }
    for (auto &t : workers)
        t.join();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "[Small+System] " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms"
              << std::endl;
}

// 2. 소형 객체 + 4KB 풀링 (Cache Pollution - Worst Case)
TEST_F(MemoryStrategyBench, Small_Pool_Pollution)
{
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> workers;
    for (int t = 0; t < THREADS; ++t)
    {
        workers.emplace_back(
            []()
            {
                for (int i = 0; i < OP_COUNT / THREADS; ++i)
                {
                    PacketMessage *p = MessagePool::AllocatePacket(sizeof(LambdaMessage));
                    LambdaMessage *m = new (p) LambdaMessage();
                    m->task = SmallWorkload();
                    m->task();
                    m->~LambdaMessage();
                    MessagePool::Free(p);
                }
            }
        );
    }
    for (auto &t : workers)
        t.join();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "[Small+Pool] " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << "ms (Pollution!)" << std::endl;
}
