cmake_minimum_required(VERSION 3.16)

project(HGSF VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    add_compile_definitions(_WIN32_WINNT=0x0A00) # Windows 10
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup VCPKG
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake") 

# Dependencies
find_package(Boost REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(concurrentqueue CONFIG REQUIRED)

# Common functionality
add_library(Share OBJECT)
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
add_library(System OBJECT)
target_sources(System PRIVATE
    src/System/Pch.h
    src/System/Log/LogImpl.cpp
    src/System/Config/CONFIG/ConfigLoader.cpp
    src/System/Memory/MemoryManager.cpp
    src/System/Memory/PacketPool.cpp
    src/System/Monitor/MONITOR/MonitorImpl.cpp
    src/System/Framework/Framework.cpp
    src/System/Thread/THREAD/ThreadPool.cpp
    src/System/Network/ASIO/AsioSession.cpp
    src/System/Network/ASIO/AsioService.cpp
    src/System/Network/ASIO/Components/Reader.cpp
    src/System/Network/ASIO/Components/Writer.cpp
    src/System/Network/ASIO/RecvBuffer.cpp
    src/System/Network/ASIO/SessionFactory.cpp
    src/System/Dispatcher/DISPATCHER/DispatcherImpl.cpp
    src/System/Debug/CrashHandler.cpp
    src/System/Timer/AsioTimer.cpp
    src/System/Timer/Timer.cpp
    # Will add files here as we create them
)
target_include_directories(System PUBLIC src/System src)
target_precompile_headers(System PRIVATE src/System/Pch.h)
target_link_libraries(System PUBLIC Share dbghelp)

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
