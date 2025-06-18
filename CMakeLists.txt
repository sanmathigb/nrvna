cmake_minimum_required(VERSION 3.10)
project(nrvna VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Adding llama.cpp dependency...")
add_subdirectory(third_party/llama.cpp)

# Core binaries
add_executable(nrvna
        cli/server.cpp
        src/server.cpp
        src/monitor.cpp
        src/runner.cpp
)

add_executable(wrk
        cli/wrk.cpp
        src/work.cpp
)

add_executable(flw
        cli/flw.cpp
        src/flow.cpp
)

# Test executables
add_executable(test_runner
        test/test_runner.cpp
        src/runner.cpp
)

add_executable(test_workflow
        test/test_workflow.cpp
        src/work.cpp
        src/flow.cpp
)

add_executable(test_monitor
        test/test_monitor.cpp
        src/runner.cpp
        src/monitor.cpp
        src/work.cpp
        src/flow.cpp
)

# Include directories
set(INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

target_include_directories(nrvna PRIVATE ${INCLUDE_DIRS})
target_include_directories(wrk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(flw PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_runner PRIVATE ${INCLUDE_DIRS})
target_include_directories(test_workflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_monitor PRIVATE ${INCLUDE_DIRS})

# Threading
find_package(Threads REQUIRED)

# Link libraries
target_link_libraries(nrvna PRIVATE llama Threads::Threads)
target_link_libraries(test_runner PRIVATE llama Threads::Threads)
target_link_libraries(test_workflow PRIVATE Threads::Threads)
target_link_libraries(test_monitor PRIVATE llama Threads::Threads)