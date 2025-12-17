cmake_minimum_required(VERSION 3.16)

# Setup VCPKG (Must be before project() to activate toolchain)
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(HGSF VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
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
    src/System/Database/DBConnectionPool.cpp # Added
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
    # Will add files here as we create them
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

# UnitTests (Consolidated Test Suite)
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(UnitTests ${TEST_SOURCES})
target_include_directories(UnitTests PRIVATE src)
target_precompile_headers(UnitTests PRIVATE src/System/Pch.h)
target_link_libraries(UnitTests PRIVATE System Share GTest::gtest GTest::gmock)

enable_testing()
include(GoogleTest)
# add_subdirectory(tests) # No longer needed as we compile sources directly
gtest_discover_tests(UnitTests)


# ==========================================
# Example: Vampire Survivor Server (Formerly SimpleGame)
# ==========================================

# Protobuf Generation
set(PROTO_SRC_DIR "src/Examples/VampireSurvivor/Proto")
set(PROTO_FILES "${PROTO_SRC_DIR}/Protocol.proto")
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_executable(VampireSurvivorServer
    src/Examples/VampireSurvivor/Server/Main.cpp
    src/Examples/VampireSurvivor/Server/Room.cpp
    ${PROTO_SRCS} ${PROTO_HDRS}
)
target_include_directories(VampireSurvivorServer PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})
target_precompile_headers(VampireSurvivorServer PRIVATE src/System/Pch.h)
target_link_libraries(VampireSurvivorServer PRIVATE System Share protobuf::libprotobuf)

# Vampire Survivor Independent Tests
file(GLOB VS_TEST_SOURCES "src/Examples/VampireSurvivor/tests/*.cpp")
add_executable(VampireSurvivorTests 
    ${VS_TEST_SOURCES}
    src/Examples/VampireSurvivor/Server/Room.cpp
    ${PROTO_SRCS} ${PROTO_HDRS}
)
target_include_directories(VampireSurvivorTests PRIVATE 
    src 
    ${CMAKE_CURRENT_BINARY_DIR}
    src/Examples/VampireSurvivor/Server
)
target_precompile_headers(VampireSurvivorTests PRIVATE src/System/Pch.h)
target_link_libraries(VampireSurvivorTests PRIVATE System Share GTest::gtest GTest::gmock protobuf::libprotobuf)
gtest_discover_tests(VampireSurvivorTests)




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
)

install(DIRECTORY src/Share DESTINATION dist/include
    FILES_MATCHING PATTERN "*.h"
)
