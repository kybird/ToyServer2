
# Google Test based tests

# ThreadPool Test
add_executable(TestThreadPool
    TestThreadPool.cpp
)

target_link_libraries(TestThreadPool
    PRIVATE
        System
        GTest::gtest_main
)
target_precompile_headers(TestThreadPool PRIVATE "${CMAKE_SOURCE_DIR}/src/System/Pch.h")

gtest_discover_tests(TestThreadPool)

# Metrics Test (Conditional)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/TestMetrics.cpp")
    add_executable(TestMetrics
        TestMetrics.cpp
    )

    target_link_libraries(TestMetrics
        PRIVATE
            System
            GTest::gtest_main
    )
    target_precompile_headers(TestMetrics PRIVATE "${CMAKE_SOURCE_DIR}/src/System/Pch.h")

    gtest_discover_tests(TestMetrics)
    gtest_discover_tests(TestMetrics)
endif()

# Async Database Test
add_executable(TestAsyncDatabase
    TestAsyncDatabase.cpp
)

target_link_libraries(TestAsyncDatabase
    PRIVATE
        System
        GTest::gtest_main
)
target_precompile_headers(TestAsyncDatabase PRIVATE "${CMAKE_SOURCE_DIR}/src/System/Pch.h")

gtest_discover_tests(TestAsyncDatabase)

# Command Console Test
add_executable(TestCommandConsole
    TestCommandConsole.cpp
)

target_link_libraries(TestCommandConsole
    PRIVATE
        System
        GTest::gtest_main
)
target_precompile_headers(TestCommandConsole PRIVATE "${CMAKE_SOURCE_DIR}/src/System/Pch.h")

gtest_discover_tests(TestCommandConsole)
