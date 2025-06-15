cmake_minimum_required(VERSION 3.10)
project(nrvna VERSION 0.1.0 LANGUAGES CXX)

# C++17 setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add llama.cpp as subdirectory - this handles everything automatically
message(STATUS "Adding llama.cpp dependency...")
add_subdirectory(third_party/llama.cpp)

# Define common source files
set(COMMON_SOURCES
        src/work.cpp
        src/flow.cpp
        src/runner.cpp
        src/server.cpp
        src/monitor.cpp
)

# Existing test executables
add_executable(test_runner
        test/test_runner.cpp
        src/runner.cpp
)

add_executable(test_workflow
        test/test_workflow.cpp
        src/work.cpp
        src/flow.cpp
)

add_executable(test_flow
        test/test_flow.cpp
        src/flow.cpp
)

add_executable(test_work
        test/test_work.cpp
        src/work.cpp
)

add_executable(test_monitor
        test/test_monitor.cpp
        src/runner.cpp
        src/monitor.cpp
        src/work.cpp
        src/flow.cpp
)

add_executable(test_servermonitor
        test/test_servermonitor.cpp
        src/server.cpp
        src/monitor.cpp
        src/runner.cpp
)

# Include directories for all targets
set(INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

# Apply includes to all targets
target_include_directories(test_runner PRIVATE ${INCLUDE_DIRS})
target_include_directories(test_workflow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_flow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_work PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(test_servermonitor PRIVATE ${INCLUDE_DIRS})
target_include_directories(test_monitor PRIVATE ${INCLUDE_DIRS})

# Threading
find_package(Threads REQUIRED)

# Link libraries
target_link_libraries(test_runner
        PRIVATE
        llama
        Threads::Threads
)

target_link_libraries(test_workflow
        PRIVATE
        Threads::Threads
)

target_link_libraries(test_flow
        PRIVATE
        Threads::Threads
)

target_link_libraries(test_work
        PRIVATE
        Threads::Threads
)

target_link_libraries(test_servermonitor
        PRIVATE
        llama
        Threads::Threads
)

target_link_libraries(test_monitor
        PRIVATE
        llama
        Threads::Threads
)