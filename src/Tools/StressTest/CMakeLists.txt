add_executable(StressTestClient 
    main.cpp
    StressTestClient.cpp
    StressTestClient.h
    ${CMAKE_SOURCE_DIR}/src/Examples/VampireSurvivor/Protocol/game.pb.cc
)

target_include_directories(StressTestClient PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/Examples/VampireSurvivor
)

target_precompile_headers(StressTestClient PRIVATE ${CMAKE_SOURCE_DIR}/src/System/Pch.h)

target_link_libraries(StressTestClient PRIVATE System Share protobuf::libprotobuf Boost::system)
