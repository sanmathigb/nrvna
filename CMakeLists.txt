cmake_minimum_required(VERSION 3.16)
project(nrvna VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(test_workflow
         test/test_workflow.cpp
         src/work.cpp
         src/flow.cpp)