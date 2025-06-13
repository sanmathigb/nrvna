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
)

# Define test executables
add_executable(test_runner
        test/test_runner.cpp
        src/runner.cpp
)

add_executable(test_workflow
        test/test_workflow.cpp
        src/work.cpp
        src/flow.cpp
)

# Include directories
target_include_directories(test_runner
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

target_include_directories(test_workflow
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Threading
find_package(Threads REQUIRED)

# Link against llama.cpp targets
target_link_libraries(test_runner
        PRIVATE
        llama
        Threads::Threads
)

target_link_libraries(test_workflow
        PRIVATE
        Threads::Threads
)