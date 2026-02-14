#include "StressTestClient.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Global shared state
std::vector<int> g_ValidRoomIds;
std::mutex g_RoomMutex;
std::atomic<int> g_CreatedRoomCount{0};

int main(int argc, char *argv[])
{
    int clientCount = 100;
    int durationSec = 60;
    int targetRoomCount = 100;

    if (argc > 1)
        clientCount = std::stoi(argv[1]);
    if (argc > 2)
        durationSec = std::stoi(argv[2]);

    std::cout << "========================================\n";
    std::cout << " Stress Test Suite (Client-Side Creation)\n";
    std::cout << " Clients: " << clientCount << "\n";
    std::cout << " Duration: " << durationSec << "s\n";
    std::cout << " Target Rooms: " << targetRoomCount << "\n";
    std::cout << "========================================\n";

    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard(io_context.get_executor());

    // Worker Threads for ASIO
    int threadCount = static_cast<int>(std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    for (int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(
            [&io_context]()
            {
                io_context.run();
            }
        );
    }

    // Phase 1: Create Rooms
    std::cout << "[Phase 1] Creating " << targetRoomCount << " rooms...\n";
    std::vector<std::shared_ptr<StressTestClient>> creators;
    creators.reserve(targetRoomCount);

    for (int i = 0; i < targetRoomCount; ++i)
    {
        auto client = std::make_shared<StressTestClient>(io_context, i);
        client->RequestCreateRoom(
            "StressRoom_" + std::to_string(i),
            [](int roomId)
            {
                std::lock_guard<std::mutex> lock(g_RoomMutex);
                g_ValidRoomIds.push_back(roomId);
                g_CreatedRoomCount++;
                if (g_CreatedRoomCount % 10 == 0)
                    std::cout << "Rooms created: " << g_CreatedRoomCount << "\r";
            }
        );
        client->Start("127.0.0.1", "9001");
        creators.push_back(client);
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // throttle creation
    }

    // Wait for creation
    int timeout = 0;
    while (g_CreatedRoomCount < targetRoomCount && timeout < 300) // Increase to 300 (30s)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout++;
    }

    std::cout << "\n[Phase 1] Completed. Created Rooms: " << g_CreatedRoomCount << "\n";

    if (g_ValidRoomIds.empty())
    {
        std::cerr << "[ERROR] No rooms created. Aborting.\n";
        return 1;
    }

    // Phase 2: Mass Injection
    std::cout << "[Phase 2] Spawning " << clientCount << " load clients...\n";
    std::vector<std::shared_ptr<StressTestClient>> loadClients;
    loadClients.reserve(clientCount);

    for (int i = 0; i < clientCount; ++i)
    {
        // Distribute among created rooms
        int roomIndex = i % g_ValidRoomIds.size();
        int targetRoomId = g_ValidRoomIds[roomIndex];

        auto client =
            std::make_shared<StressTestClient>(io_context, 1000 + i); // Offset ID to avoid conflict with creators
        client->SetTargetRoom(targetRoomId);
        client->Start("127.0.0.1", "9001");

        loadClients.push_back(client);

        if (i % 500 == 0)
        {
            std::cout << "Spawned " << i << " clients...\r";
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Avoid SYN flood
        }
    }
    std::cout << "\n[Phase 2] All clients spawned.\n";

    // Monitor Loop
    auto startTime = std::chrono::steady_clock::now();
    while (true)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        if (elapsed >= durationSec)
            break;

        int connected = 0;
        int loggedIn = 0;
        int inRoom = 0;

        // Check Load Clients
        for (const auto &c : loadClients)
        {
            if (c->IsConnected())
                connected++;
            if (c->IsLoggedIn())
                loggedIn++;
            if (c->IsInRoom())
                inRoom++;
            c->Update();
        }

        std::cout << "[StressTest] Time: " << elapsed << "s | Con: " << connected << " | Room: " << inRoom
                  << " | ValidRooms: " << g_ValidRoomIds.size() << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    std::cout << "Stopping clients...\n";
    for (const auto &c : creators)
        c->Stop();
    for (const auto &c : loadClients)
        c->Stop();

    workGuard.reset();
    io_context.stop();
    for (auto &t : threads)
        t.join();

    std::cout << "Test Finished.\n";
    return 0;
}
