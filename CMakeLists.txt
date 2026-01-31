cmake_minimum_required(VERSION 3.16)

# Setup VCPKG (Must be before project() to activate toolchain)
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed directory")
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")

project(HGSF VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
    add_compile_options(/FS /utf-8) # Enable synchronous PDB access to fix C1041, and UTF-8 for fmt
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Setup VCPKG



# ==========================================
# Options
# ==========================================
option(ENABLE_DRIVER_MYSQL "Enable MySQL Database Driver" OFF)

# Dependencies
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

if(ENABLE_DRIVER_MYSQL)
    find_package(MySQL REQUIRED)
    find_package(unofficial-libmysql CONFIG REQUIRED)
endif()

find_package(cnats CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)



# Common functionality
add_library(Share STATIC)
target_sources(Share PRIVATE 
    src/System/Pch.h
    src/Share/Protocol.cpp
)
target_include_directories(Share PUBLIC src/Share src/System src)
target_precompile_headers(Share PRIVATE src/System/Pch.h)
target_link_libraries(Share PRIVATE 
    Boost::system
    spdlog::spdlog 
    nlohmann_json::nlohmann_json 
    fmt::fmt
    concurrentqueue::concurrentqueue
)

# ==========================================
# Drivers (Integrated into System)
# ==========================================
# Drivers are now statically linked into System library.
# See System target definition below.



# System Library (The Engine)
add_library(System STATIC)
target_sources(System PRIVATE
    src/System/Pch.h
    src/System/Log/LogImpl.cpp
    src/System/Config/Json/JsonConfigLoader.cpp
    src/System/Memory/MemoryManager.cpp
    src/System/Metrics/MetricsCollector.cpp # Refactored from Monitor
    # Session (Refactored)
    src/System/Session/Session.cpp
    src/System/Session/Session.h
    src/System/Session/SessionFactory.cpp
    src/System/Session/SessionFactory.h
    src/System/Session/SessionPool.h
    src/System/Session/GatewaySession.cpp
    src/System/Session/GatewaySession.h
    src/System/Session/BackendSession.cpp
    src/System/Session/BackendSession.h
    src/System/Session/SessionCommon.h
    src/System/Session/SessionContext.cpp
    src/System/Session/SessionContext.h
    src/System/ISession.h

    src/System/Network/INetwork.h
    src/System/Network/NetworkImpl.cpp
    src/System/Network/RecvBuffer.cpp
    src/System/Network/RateLimiter.h
    src/System/Events/EventBus.h
    src/System/Network/ByteBuffer.h
    
    src/System/Database/DatabaseImpl.cpp
    src/System/Database/DatabaseRegistry.h
    src/System/Database/DatabaseRegistry.cpp
    
    src/System/Framework/Framework.cpp
    src/System/Thread/ThreadPool.cpp
    src/System/Dispatcher/DISPATCHER/DispatcherImpl.cpp
    src/System/Dispatcher/MessagePool.cpp
    src/System/Debug/CrashHandler.cpp
    src/System/Debug/MemoryMetrics.cpp
    src/System/Timer/TimerImpl.cpp
    src/System/ITimer.h
    src/System/Timer/TimerHandle.h
    src/System/Console/CommandConsole.h
    src/System/Console/CommandConsole.cpp
    
    src/System/ToyServerSystem.h
    src/System/Network/PacketUtils.cpp
    src/System/Network/PacketUtils.h
    src/System/Dispatcher/SystemMessages.h
    src/System/PacketView.h
    src/System/Network/XorEncryption.h
    src/System/Network/AesEncryption.h
    src/System/Network/AesEncryption.cpp
    
    src/System/Internal/PacketStorage.h
    src/System/Internal/PacketStorage.cpp
    src/System/Packet/PacketBuilder.h
    src/System/Packet/SharedPacket.h
    src/System/Packet/PacketBroadcast.h
    
    src/System/MQ/IMessageDriver.h
    src/System/MQ/MessageQoS.h
    src/System/MQ/NatsDriver.h
    src/System/MQ/NatsDriver.cpp
    src/System/MQ/RedisStreamDriver.h
    src/System/MQ/RedisStreamDriver.cpp
    src/System/MQ/MessageSystem.h
    src/System/MQ/MessageSystem.cpp

    # SQLite (Always Included)
    src/System/Drivers/SQLite/SQLiteConnection.cpp
    src/System/Drivers/SQLite/SQLiteDriver.cpp
)

# MySQL (Optional)
if(ENABLE_DRIVER_MYSQL)
    target_sources(System PRIVATE
        src/System/Drivers/MySQL/MySQLConnection.cpp
        src/System/Drivers/MySQL/MySQLDriver.cpp
    )
    target_compile_definitions(System PUBLIC USE_MYSQL)
endif()

# Always define USE_SQLITE as it is default
target_compile_definitions(System PUBLIC USE_SQLITE)

target_include_directories(System PUBLIC src/System src)
target_precompile_headers(System PRIVATE src/System/Pch.h)
target_link_libraries(System PUBLIC Share unofficial::sqlite3::sqlite3 cnats::nats redis++::redis++ PRIVATE Boost::system)

# Windows-specific dependencies
if(WIN32)
    target_link_libraries(System PUBLIC dbghelp)
endif()

if(ENABLE_DRIVER_MYSQL)
    target_link_libraries(System PUBLIC unofficial::libmysql::libmysql)
endif()

# GameServer Executable
add_executable(GameServer src/GameServer/main.cpp)
target_include_directories(GameServer PRIVATE src/GameServer src)
target_precompile_headers(GameServer PRIVATE src/System/Pch.h)
target_link_libraries(GameServer PRIVATE System Share)



# Copy Data Directory to Output Directory
add_custom_command(TARGET GameServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:GameServer>/data"
)

# DummyClient Executable
add_executable(DummyClient src/DummyClient/main.cpp)
target_include_directories(DummyClient PRIVATE src/DummyClient src)
target_precompile_headers(DummyClient PRIVATE src/System/Pch.h)
target_link_libraries(DummyClient PRIVATE System Share)

# Packet Test Executable
add_executable(PacketTest src/Tools/PacketTest/PacketTest.cpp)
target_include_directories(PacketTest PRIVATE src)
target_precompile_headers(PacketTest PRIVATE src/System/Pch.h)
target_link_libraries(PacketTest PRIVATE System Share)





# ==========================================
# Example: Vampire Survivor Server (Formerly SimpleGame)
# ==========================================

# Protobuf Generation
set(PROTO_SRC_DIR "src/Examples/VampireSurvivor/Protocol")
set(PROTO_FILES "${PROTO_SRC_DIR}/game.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Common Game Sources
set(GAME_SOURCES
    src/Examples/VampireSurvivor/Server/Core/ServerApp.cpp
    src/Examples/VampireSurvivor/Server/Core/UserDB.cpp
    src/Examples/VampireSurvivor/Server/Core/LoginController.cpp
    src/Examples/VampireSurvivor/Server/Core/GamePacketHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Auth/LoginHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Lobby/EnterLobbyHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Room/CreateRoomHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Room/GetRoomListHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Room/JoinRoomHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Room/LeaveRoomHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Game/MoveInputHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Game/GameReadyHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Game/ChatHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/Game/SelectLevelUpHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/System/PingHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/Handlers/System/PongHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/DataManager.cpp
    src/Examples/VampireSurvivor/Server/Game/Room.cpp
    src/Examples/VampireSurvivor/Server/Game/RoomManager.cpp
    src/Examples/VampireSurvivor/Server/Game/WaveManager.cpp
    src/Examples/VampireSurvivor/Server/Game/DamageEmitter.cpp
    src/Examples/VampireSurvivor/Server/Game/CombatManager.cpp
    src/Examples/VampireSurvivor/Server/Game/LevelUpManager.cpp
    src/Examples/VampireSurvivor/Server/Game/Effect/EffectManager.cpp
    src/Examples/VampireSurvivor/Server/Entity/GameObject.cpp
    src/Examples/VampireSurvivor/Server/Entity/Monster.cpp
    src/Examples/VampireSurvivor/Server/Entity/ModifierContainer.cpp
    src/Examples/VampireSurvivor/Server/Entity/MonsterFactory.cpp
    src/Examples/VampireSurvivor/Server/Entity/Player.cpp
    src/Examples/VampireSurvivor/Server/Entity/PlayerFactory.cpp
    src/Examples/VampireSurvivor/Server/Entity/PlayerInventory.cpp
    src/Examples/VampireSurvivor/Server/Entity/AI/ChaserAI.cpp
    src/Examples/VampireSurvivor/Server/Entity/AI/WanderAI.cpp
    src/Examples/VampireSurvivor/Server/Entity/AI/SwarmAI.cpp
    src/Examples/VampireSurvivor/Server/Entity/AI/BossAI.cpp
    src/Examples/VampireSurvivor/Server/Entity/ProjectileFactory.cpp
    ${PROTO_SRCS} ${PROTO_HDRS}
)

add_executable(VampireSurvivorServer
    src/Examples/VampireSurvivor/Server/Main.cpp
    ${GAME_SOURCES}
)

target_include_directories(VampireSurvivorServer PRIVATE 
    src 
    ${CMAKE_CURRENT_BINARY_DIR}
    src/Examples/VampireSurvivor/Server
    src/Examples/VampireSurvivor
    src/Examples/VampireSurvivor/Common
    src/Examples/VampireSurvivor/Server/Core
)

target_precompile_headers(VampireSurvivorServer PRIVATE src/System/Pch.h)

target_link_libraries(VampireSurvivorServer PRIVATE System Share protobuf::libprotobuf)



# Copy Data Directory to Output Directory for VampireSurvivorServer
add_custom_command(TARGET VampireSurvivorServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:VampireSurvivorServer>/data"
)

# Vampire Survivor Console Client (DEPRECATED - Use Unity Client)
# add_executable(VampireSurvivorClient src/Examples/VampireSurvivor/Client/ConsoleClient.cpp ${PROTO_SRCS} ${PROTO_HDRS})
# target_include_directories(VampireSurvivorClient PRIVATE src/Examples/VampireSurvivor src src/Examples/VampireSurvivor/Common)
# target_link_libraries(VampireSurvivorClient PRIVATE System Share protobuf::libprotobuf)

# TickSyncClient Executable
add_executable(TickSyncClient src/TickSyncClient/main.cpp ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(TickSyncClient PRIVATE src/TickSyncClient src ${CMAKE_CURRENT_BINARY_DIR} src/Examples/VampireSurvivor)
target_precompile_headers(TickSyncClient PRIVATE src/System/Pch.h)
target_link_libraries(TickSyncClient PRIVATE System Share protobuf::libprotobuf)




# Install Configuration
install(TARGETS System Share
    ARCHIVE DESTINATION dist/lib
    LIBRARY DESTINATION dist/lib
    RUNTIME DESTINATION dist/bin
)

install(DIRECTORY src/System DESTINATION dist/include
    FILES_MATCHING PATTERN "*.h"
    PATTERN "BOOTSTRAP" EXCLUDE
    PATTERN "System/Dispatcher/MessagePool.h" EXCLUDE
    PATTERN "System/Dispatcher/SystemMessages.h" EXCLUDE
    PATTERN "System/Database/*.h" EXCLUDE # Internal headers
)

install(DIRECTORY src/Share DESTINATION dist/include
    FILES_MATCHING PATTERN "*.h"
)

# Force CMake Re-scan

# UnitTests (Consolidated Test Suite)
set(VS_TEST_SOURCES
    src/Examples/VampireSurvivor/tests/main_test.cpp
    src/Examples/VampireSurvivor/tests/TestRoom.cpp
    src/Examples/VampireSurvivor/tests/TestRoom.cpp
    src/Examples/VampireSurvivor/tests/TestCombat.cpp
    src/Examples/VampireSurvivor/tests/TestDeadReckoningCorrection.cpp
    src/Examples/VampireSurvivor/tests/TestEffectSystem.cpp
    tests/TestEventBus_Mock.cpp
    tests/TestAsyncDatabase.cpp
    tests/TestThreadPool.cpp
    tests/TestByteBuffer.cpp
    tests/TestRateLimiter.cpp
    tests/TestSpatialGrid.cpp
    tests/TestSpatialGrid.cpp
    tests/TestPerformance.cpp
    tests/TestMQ.cpp
    src/Examples/VampireSurvivor/tests/TestStatModifier.cpp
    tests/TestBroadcastBenchmark.cpp
    tests/TestDispatcherBenchmark.cpp
    tests/TestGatherWriteBenchmark.cpp
    tests/TestMessagePoolExpansion.cpp
    tests/TestSmartNotifyBenchmark.cpp
    tests/TestMemoryBench.cpp
)
add_executable(UnitTests ${VS_TEST_SOURCES} ${GAME_SOURCES})
target_include_directories(UnitTests PRIVATE 
    src
    src/Examples/VampireSurvivor/Server
    src/Examples/VampireSurvivor
    src/Examples/VampireSurvivor/Common
    src/Examples/VampireSurvivor/Server/Core
)
target_precompile_headers(UnitTests PRIVATE src/System/Pch.h)
target_link_libraries(UnitTests PRIVATE System Share GTest::gtest GTest::gmock protobuf::libprotobuf)



# Copy Data Directory to Output Directory for UnitTests
add_custom_command(TARGET UnitTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/data"
    "$<TARGET_FILE_DIR:UnitTests>/data"
)

enable_testing()
include(GoogleTest)
# add_subdirectory(tests) # No longer needed as we compile sources directly
gtest_discover_tests(UnitTests)
