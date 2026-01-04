cmake_minimum_required(VERSION 3.16)

# Setup VCPKG (Must be before project() to activate toolchain)
set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed directory")
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(HGSF VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
    add_compile_options(/FS) # Enable synchronous PDB access to fix C1041
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup VCPKG


# Dependencies
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(Protobuf REQUIRED)

# Common functionality
add_library(Share STATIC)
target_sources(Share PRIVATE 
    src/System/Pch.h
    src/Share/Protocol.cpp
)
target_include_directories(Share PUBLIC src/Share src/System src)
target_precompile_headers(Share PRIVATE src/System/Pch.h)
target_link_libraries(Share PUBLIC 
    Boost::system
    spdlog::spdlog 
    nlohmann_json::nlohmann_json 
    fmt::fmt
    concurrentqueue::concurrentqueue
)

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
    src/System/ISession.h

    src/System/Network/INetwork.h
    src/System/Network/NetworkImpl.cpp
    src/System/Network/RecvBuffer.cpp
    src/System/Network/RateLimiter.h # Added
    src/System/Events/EventBus.h
    src/System/Network/ByteBuffer.h # Added
    src/System/Database/DatabaseImpl.cpp
    src/System/Database/SQLite/SQLiteConnection.cpp
    src/System/Framework/Framework.cpp
    src/System/Thread/ThreadPool.cpp
    src/System/Dispatcher/DISPATCHER/DispatcherImpl.cpp
    src/System/Dispatcher/MessagePool.cpp
    src/System/Debug/CrashHandler.cpp
    src/System/Debug/MemoryMetrics.cpp
    src/System/Timer/TimerImpl.cpp
    src/System/ITimer.h
    src/System/Timer/TimerHandle.h
    src/System/Console/CommandConsole.h # Added
    src/System/Console/CommandConsole.cpp # Added
    
    # New Refactoring
    src/System/ToyServerSystem.h
    src/System/Network/PacketUtils.cpp
    src/System/Network/PacketUtils.h
    src/System/Dispatcher/SystemMessages.h
    src/System/PacketView.h
    src/System/Network/XorEncryption.h
    src/System/Network/AesEncryption.h
    src/System/Network/AesEncryption.cpp
    
    # Phase 3 Internal
    src/System/Internal/PacketStorage.h
    src/System/Internal/PacketStorage.cpp
    src/System/Packet/PacketBuilder.h
    src/System/Packet/SharedPacket.h
    src/System/Packet/PacketBroadcast.h
)
target_include_directories(System PUBLIC src/System src)
target_precompile_headers(System PRIVATE src/System/Pch.h)
target_link_libraries(System PUBLIC Share dbghelp unofficial::sqlite3::sqlite3)

# GameServer Executable
add_executable(GameServer src/GameServer/main.cpp)
target_include_directories(GameServer PRIVATE src/GameServer src)
target_precompile_headers(GameServer PRIVATE src/System/Pch.h)
target_link_libraries(GameServer PRIVATE System Share)

# Copy Config File to Output Directory
add_custom_command(TARGET GameServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/server_config.json"
    "$<TARGET_FILE_DIR:GameServer>/server_config.json"
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
    src/Examples/VampireSurvivor/Server/Core/LoginController.cpp
    src/Examples/VampireSurvivor/Server/Core/GamePacketHandler.cpp
    src/Examples/VampireSurvivor/Server/Core/DataManager.cpp
    src/Examples/VampireSurvivor/Server/Game/Room.cpp
    src/Examples/VampireSurvivor/Server/Game/RoomManager.cpp
    src/Examples/VampireSurvivor/Server/Game/WaveManager.cpp
    src/Examples/VampireSurvivor/Server/Entity/Monster.cpp
    src/Examples/VampireSurvivor/Server/Entity/MonsterFactory.cpp
    src/Examples/VampireSurvivor/Server/Entity/PlayerFactory.cpp
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
)

target_link_libraries(VampireSurvivorServer PRIVATE System Share protobuf::libprotobuf)

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
    src/Examples/VampireSurvivor/tests/TestCombat.cpp
    src/Examples/VampireSurvivor/tests/TestSwarmPerformance.cpp
    src/Examples/VampireSurvivor/tests/TestDeadReckoningCorrection.cpp
)
add_executable(UnitTests ${VS_TEST_SOURCES} ${GAME_SOURCES})
target_include_directories(UnitTests PRIVATE 
    src
    src/Examples/VampireSurvivor/Server
    src/Examples/VampireSurvivor
    src/Examples/VampireSurvivor/Common
)
target_precompile_headers(UnitTests PRIVATE src/System/Pch.h)
target_link_libraries(UnitTests PRIVATE System Share GTest::gtest GTest::gmock protobuf::libprotobuf unofficial::sqlite3::sqlite3)

enable_testing()
include(GoogleTest)
# add_subdirectory(tests) # No longer needed as we compile sources directly
gtest_discover_tests(UnitTests)
