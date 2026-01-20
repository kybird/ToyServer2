#include "System/Dispatcher/MessagePool.h"
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>


using namespace System;
using boost::asio::ip::tcp;

struct CPUShot
{
    ULONGLONG userTime;
    ULONGLONG kernelTime;

    static CPUShot Take()
    {
        FILETIME createTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime))
        {
            ULARGE_INTEGER u, k;
            u.LowPart = userTime.dwLowDateTime;
            u.HighPart = userTime.dwHighDateTime;
            k.LowPart = kernelTime.dwLowDateTime;
            k.HighPart = kernelTime.dwHighDateTime;
            return {u.QuadPart / 10, k.QuadPart / 10}; // 100ns -> us
        }
        return {0, 0};
    }
};

class GatherWriteIoBenchmark : public ::testing::Test
{
protected:
    const int BATCH_SIZE = 50; // 배치 크기를 약간 줄여 소켓 버퍼 안정성 확보
    boost::asio::io_context io_context;
    tcp::acceptor *acceptor;
    tcp::socket *serverSideSocket;
    tcp::socket *clientSideSocket;
    std::thread *drainThread;
    std::atomic<bool> isRunning{false};

    void SetUp() override
    {
        MessagePool::Prepare(2000);
        acceptor = new tcp::acceptor(io_context, tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));
        clientSideSocket = new tcp::socket(io_context);
        serverSideSocket = new tcp::socket(io_context);
        clientSideSocket->connect(acceptor->local_endpoint());
        acceptor->accept(*serverSideSocket);

        isRunning = true;
        drainThread = new std::thread(
            [this]()
            {
                std::vector<char> dummy(1024 * 1024);
                boost::system::error_code ec;
                while (isRunning)
                {
                    serverSideSocket->read_some(boost::asio::buffer(dummy), ec);
                    if (ec)
                        break;
                }
            }
        );

        boost::asio::socket_base::send_buffer_size send_option(1024 * 1024);
        clientSideSocket->set_option(send_option);
    }

    void TearDown() override
    {
        isRunning = false;
        if (clientSideSocket->is_open())
            clientSideSocket->close();
        if (serverSideSocket->is_open())
            serverSideSocket->close();
        if (drainThread->joinable())
            drainThread->join();
        delete drainThread;
        delete clientSideSocket;
        delete serverSideSocket;
        delete acceptor;
        MessagePool::Clear();
    }

    void RunTest(const std::string &name, int packetSize, int iterations, bool useGather)
    {
        std::vector<uint8_t> linearBuffer(BATCH_SIZE * packetSize);
        std::vector<boost::asio::const_buffer> buffers;
        buffers.reserve(BATCH_SIZE);

        auto startWall = std::chrono::high_resolution_clock::now();
        auto startCPU = CPUShot::Take();

        for (int it = 0; it < iterations; ++it)
        {
            std::vector<PacketMessage *> items;
            for (int i = 0; i < BATCH_SIZE; ++i)
            {
                items.push_back(MessagePool::AllocatePacket(packetSize));
            }

            if (useGather)
            {
                buffers.clear();
                for (int i = 0; i < BATCH_SIZE; ++i)
                {
                    buffers.push_back(boost::asio::buffer(items[i]->Payload(), items[i]->length));
                }
                boost::asio::write(*clientSideSocket, buffers);
            }
            else
            {
                uint8_t *destPtr = linearBuffer.data();
                for (int i = 0; i < BATCH_SIZE; ++i)
                {
                    std::memcpy(destPtr, items[i]->Payload(), items[i]->length);
                    destPtr += items[i]->length;
                }
                boost::asio::write(*clientSideSocket, boost::asio::buffer(linearBuffer));
            }

            for (auto p : items)
                MessagePool::Free(p);
        }

        auto endCPU = CPUShot::Take();
        auto endWall = std::chrono::high_resolution_clock::now();
        auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(endWall - startWall).count();

        std::cout << "[" << name << " | " << packetSize << "B] Wall: " << wallMs << "ms, "
                  << "User: " << (endCPU.userTime - startCPU.userTime) / 1000.0 << "ms, "
                  << "Kernel: " << (endCPU.kernelTime - startCPU.kernelTime) / 1000.0 << "ms" << std::endl;
    }
};

TEST_F(GatherWriteIoBenchmark, Compare4KB)
{
    int size = 4000;
    int iters = 1000;
    RunTest("Legacy", size, iters, false);
    RunTest("Gather", size, iters, true);
}

// Huge Packet 테스트 (풀 우회 시뮬레이션)
TEST_F(GatherWriteIoBenchmark, Compare64KB)
{
    int size = 64000;
    int iters = 100; // 전송량이 크므로 횟수 조절

    // Legacy 64KB
    {
        std::vector<uint8_t> linearBuffer(BATCH_SIZE * size);
        auto startWall = std::chrono::high_resolution_clock::now();
        auto startCPU = CPUShot::Take();
        for (int it = 0; it < iters; ++it)
        {
            std::vector<std::vector<uint8_t>> items(BATCH_SIZE, std::vector<uint8_t>(size, 0xAF));
            uint8_t *destPtr = linearBuffer.data();
            for (int i = 0; i < BATCH_SIZE; ++i)
            {
                std::memcpy(destPtr, items[i].data(), size);
                destPtr += size;
            }
            boost::asio::write(*clientSideSocket, boost::asio::buffer(linearBuffer));
        }
        auto endCPU = CPUShot::Take();
        auto endWall = std::chrono::high_resolution_clock::now();
        auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(endWall - startWall).count();
        std::cout << "[Legacy | 64KB] Wall: " << wallMs
                  << "ms, User: " << (endCPU.userTime - startCPU.userTime) / 1000.0
                  << "ms, Kernel: " << (endCPU.kernelTime - startCPU.kernelTime) / 1000.0 << "ms" << std::endl;
    }

    // Gather 64KB
    {
        auto startWall = std::chrono::high_resolution_clock::now();
        auto startCPU = CPUShot::Take();
        for (int it = 0; it < iters; ++it)
        {
            std::vector<std::vector<uint8_t>> items(BATCH_SIZE, std::vector<uint8_t>(size, 0xAF));
            std::vector<boost::asio::const_buffer> buffers;
            for (int i = 0; i < BATCH_SIZE; ++i)
            {
                buffers.push_back(boost::asio::buffer(items[i].data(), size));
            }
            boost::asio::write(*clientSideSocket, buffers);
        }
        auto endCPU = CPUShot::Take();
        auto endWall = std::chrono::high_resolution_clock::now();
        auto wallMs = std::chrono::duration_cast<std::chrono::milliseconds>(endWall - startWall).count();
        std::cout << "[Gather | 64KB] Wall: " << wallMs
                  << "ms, User: " << (endCPU.userTime - startCPU.userTime) / 1000.0
                  << "ms, Kernel: " << (endCPU.kernelTime - startCPU.kernelTime) / 1000.0 << "ms" << std::endl;
    }
}
