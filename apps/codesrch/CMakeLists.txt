cmake_minimum_required(VERSION 3.16)
project(codesrch LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(codesrch src/main.cpp src/codesrch.cpp)
target_include_directories(codesrch PRIVATE
    src
    ../../third_party/llama.cpp/vendor
)
target_compile_definitions(codesrch PRIVATE NRVNA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_options(codesrch PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(codesrch_test tests/codesrch_test.cpp src/codesrch.cpp)
    target_include_directories(codesrch_test PRIVATE src ../../third_party/llama.cpp/vendor)
    target_compile_options(codesrch_test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME codesrch_unit COMMAND codesrch_test)
endif()
