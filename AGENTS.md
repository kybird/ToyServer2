# AGENTS.md - Development Guide for ToyServer2

This document provides essential information for AI agents and developers working on the ToyServer2 codebase. It covers build commands, testing procedures, code style guidelines, and project conventions.

## Build Commands

### Full Project Build
```bash
# Windows (recommended)
rebuild.bat

# Manual build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_INSTALLED_DIR=%cd%/vcpkg_installed
cmake --build build --config Debug
```

### Release Build
```bash
build_release.bat
```

### Specific Target Builds
```bash
# Vampire Survivor Server only
build.bat

# Test build (Linux)
build_tests.sh
```

## Testing Commands

### Run All C++ Unit Tests
```bash
# Windows
RunTests.bat

# Manual execution
cd build/Debug
UnitTests.exe

# With CTest (formal verification)
verification.bat
```

### Run Single C++ Test
```bash
# Specific test suite
build/Debug/UnitTests.exe --gtest_filter=ThreadPoolTest.*

# Specific test case
build/Debug/UnitTests.exe --gtest_filter=ThreadPoolTest.SimpleTask
```

### Run Unity Client Tests
```bash
run_unity_test.bat
```

### Run Stress Tests
```bash
# Debug build (1000 clients, 10 seconds)
stress_test.bat

# Release build
stress_test_release.bat
```

### Run Individual Test Executables
```bash
# Available individual test executables in build/Debug:
TestThreadPool.exe
TestAsyncDatabase.exe
TestMetrics.exe
# ... and others
```

## Code Formatting & Linting

### Format Code
```bash
# Format all C++ files
clang-format -i --style=file src/**/*.cpp src/**/*.h tests/**/*.cpp tests/**/*.h

# Check formatting without changes
clang-format --dry-run --style=file src/**/*.cpp src/**/*.h tests/**/*.cpp tests/**/*.h
```

### Lint Code (IDE Integration)
- **CLion/CLion**: Uses `.clang-format` automatically
- **VSCode**: Install C/C++ extension, configure clang-format
- **Visual Studio**: Built-in clang-format support with `.clang-format`

## Code Style Guidelines

### Language & Standards
- **C++ Version**: C++20 (required)
- **Compiler**: MSVC v143+, Clang 10+, GCC 10+
- **Build System**: CMake 3.20+ with vcpkg

### Naming Conventions

#### Classes & Types
- **Regular Classes**: PascalCase (e.g., `ThreadPool`, `NetworkImpl`)
- **Interfaces**: `I` prefix + PascalCase (e.g., `ISession`, `ILog`, `IDispatcher`)
- **Structs**: PascalCase (e.g., `PacketHeader`)
- **Enums**: PascalCase (e.g., `LogLevel`, `SessionState`)

#### Variables & Members
- **Member Variables**: `_` prefix + camelCase (e.g., `_threadCount`, `_ioContext`)
- **Local Variables**: camelCase (e.g., `packetSize`, `sessionId`)
- **Global/Static**: `g_` prefix + PascalCase (rarely used, prefer DI)
- **Constants**: UPPER_SNAKE_CASE (e.g., `MAX_CONNECTIONS`)

#### Functions & Methods
- **Public Methods**: PascalCase (e.g., `Start()`, `SendPacket()`, `GetId()`)
- **Private Methods**: camelCase (e.g., `startAccept()`, `processPacket()`)
- **Free Functions**: PascalCase (e.g., `CreateSession()`)

#### Namespaces
- **Primary**: `System` (core framework), `SimpleGame` (game logic)
- **Avoid**: `using namespace` declarations in header files

### Code Structure & Formatting

#### Braces & Indentation
- **Brace Style**: Allman (braces on new lines)
- **Indentation**: 4 spaces (no tabs)
- **Column Limit**: 120 characters
- **Access Modifiers**: Indented with 4 spaces, no extra indentation

```cpp
class NetworkImpl
{
public:
    bool Start(uint16_t port)
    {
        try
        {
            // implementation
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Network Start Failed: {}", e.what());
            return false;
        }
    }
};
```

#### Includes & Imports
- **Header Order**:
  1. Related header file (.h for .cpp)
  2. System headers (<iostream>, <vector>, etc.)
  3. Third-party headers (<boost/asio.hpp>, <spdlog/spdlog.h>)
  4. Project headers ("System/ILog.h", "System/Pch.h")
- **Precompiled Headers**: Always include "System/Pch.h" first in .cpp files
- **Include Guards**: `#pragma once` (preferred over traditional guards)

#### Error Handling
- **Exceptions**: Use try/catch blocks, log errors with `LOG_ERROR`
- **Return Values**: Prefer Result<T> or std::expected for operations that can fail
- **Assertions**: Use sparingly, mainly in debug builds
- **RAII**: Always use RAII for resource management

```cpp
try
{
    // risky operation
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);
    _acceptor.bind(endpoint);
}
catch (const std::exception& e)
{
    LOG_ERROR("Network binding failed: {}", e.what());
    return false;
}
```

### Architectural Patterns

#### Dependency Injection (DI)
- **Preferred**: Constructor injection over singletons
- **Example**: `AsioSession` receives `IDispatcher*` in constructor
- **Factory Pattern**: Use factories for object creation
- **Service Locator**: Avoid global service locators

#### Interface Segregation
- **Public APIs**: Only interfaces (`I*`) in public headers
- **Implementation Hiding**: Keep implementations in private subdirectories
- **Directory Structure**:
  ```
  src/System/Dispatcher/
  ├── IDispatcher.h          // Public interface
  └── DISPATCHER/            // Private implementation
      ├── DispatcherImpl.h
      └── DispatcherImpl.cpp
  ```

#### RAII & Smart Pointers
- **Ownership**: Use `std::unique_ptr` for exclusive ownership
- **Shared Resources**: Use `std::shared_ptr` for shared ownership
- **References**: Prefer references over pointers for parameters
- **Raw Pointers**: Only for performance-critical code or C API interop

#### Hot Path Optimization
- **Critical Sections**: No locks, allocations, or complex logic in IO threads
- **Notification Pattern**: Hot paths only notify, actual work done in logic threads
- **Lock-Free**: Use lock-free data structures where possible
- **O(1) Operations**: Keep hot path complexity constant

### Database Rules
- **Transactions**: Always use RAII `ITransaction`
- **Ownership Transfer**: Use `std::move()` when taking ownership
- **Auto-Rollback**: Transactions auto-rollback on scope exit unless committed

```cpp
auto res = db->BeginTransaction();
if (res.status.IsOk())
{
    auto tx = std::move(*res.value);  // Take ownership
    // ... database operations
    tx->Commit();  // Explicit commit
} // Auto-rollback if not committed
```

### Testing Patterns

#### Google Test Structure
- **Test Classes**: Inherit from `::testing::Test`
- **Setup/TearDown**: Use `SetUp()` and `TearDown()` methods
- **Test Naming**: `TEST_F(TestFixture, TestName)`

```cpp
class ThreadPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup code
    }

    void TearDown() override
    {
        // Cleanup code
    }
};

TEST_F(ThreadPoolTest, SimpleTask)
{
    System::ThreadPool pool(2);
    pool.Start();

    auto future = pool.Enqueue([]() { return 42; });
    EXPECT_EQ(future.get(), 42);

    pool.Stop();
}
```

#### Unity Testing
- **Test Location**: `Assets/Tests/` directory
- **Framework**: Unity Test Framework
- **Execution**: Via Test Runner window or `run_unity_test.bat`

### Project Structure

#### Key Directories
- `src/System/`: Core framework (Network, Thread, Memory, etc.)
- `src/GameServer/`: Main server application
- `src/DummyClient/`: Stress testing client
- `src/Examples/VampireSurvivor/`: Game-specific logic
- `tests/`: C++ unit tests
- `data/`: Configuration files and databases
- `logs/`: Application logs

#### File Organization
- **Headers**: `.h` files in appropriate directories
- **Implementation**: `.cpp` files alongside headers where possible
- **Tests**: Mirror source structure in `tests/` or game-specific test directories

### Development Workflow

#### Adding New Features
1. **Plan**: Create interfaces first (`I*` headers)
2. **Implement**: Add implementations in appropriate subdirectories
3. **Test**: Write unit tests following existing patterns
4. **Build**: Run full test suite before committing
5. **Document**: Update relevant documentation

#### Code Review Checklist
- [ ] Follows naming conventions
- [ ] Uses RAII and smart pointers appropriately
- [ ] Includes proper error handling
- [ ] No violations of hot path rules
- [ ] Tests added for new functionality
- [ ] Code formatted with clang-format
- [ ] No `using namespace` in headers

### Tooling & Dependencies

#### Required Tools
- **CMake**: 3.20+
- **vcpkg**: For dependency management
- **C++ Compiler**: MSVC, Clang, or GCC with C++20 support
- **clang-format**: For code formatting

#### Key Dependencies
- **Boost.Asio**: Networking
- **spdlog**: Logging
- **nlohmann_json**: JSON handling
- **Google Test**: Unit testing
- **SQLite**: Database
- **Protobuf**: Serialization

### Performance Guidelines

#### Memory Management
- **Avoid Allocations**: In hot paths (network IO, timer callbacks)
- **Pool Objects**: Use object pools for frequently allocated objects
- **Stack Allocation**: Prefer stack over heap where possible

#### Concurrency
- **Thread Safety**: Document thread-safety guarantees
- **Lock Granularity**: Keep locks as fine-grained as possible
- **Deadlock Prevention**: Consistent lock ordering

### Common Pitfalls to Avoid

- **Manager Suffix**: Don't create `PacketManager`, `SessionManager` classes
- **Global State**: Avoid singletons, prefer dependency injection
- **Raw Pointers**: Don't return raw pointers from functions
- **Exception Specifications**: Don't use `throw()` or `noexcept` incorrectly
- **Include What You Use**: Include all necessary headers
- **Namespace Pollution**: Don't use `using namespace` in global scope

---

This guide ensures consistency across the codebase. When in doubt, refer to existing code patterns or the `doc/coding_convention.md` file for additional details.