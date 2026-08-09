cmake_minimum_required(VERSION 3.16)
project(nrvna VERSION 0.1.0 LANGUAGES CXX C)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# Add llama.cpp submodule
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_NUMBER 0 CACHE STRING "" FORCE)
set(LLAMA_INSTALL_VERSION "0.0.0")
set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
add_subdirectory(third_party/llama.cpp)

# Add mtmd library for multimodal support
add_subdirectory(third_party/llama.cpp/tools/mtmd)
foreach(_tgt llama-mtmd-cli llama-mtmd-debug llama-llava-cli llama-gemma3-cli llama-minicpmv-cli llama-qwen2vl-cli)
    if (TARGET ${_tgt})
        set_target_properties(${_tgt} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()

# Core library sources
set(NRVNA_SOURCES
    src/logger.cpp
    src/work.cpp
    src/scanner.cpp
    src/pool.cpp
    src/processor.cpp
    src/server.cpp
    src/flow.cpp
    src/runner.cpp
    src/runner_tts.cpp
    src/meta.cpp
    src/lifecycle.cpp
)

# Core library
add_library(nrvna_core STATIC ${NRVNA_SOURCES})
if(TARGET llama-common)
    set(NRVNA_LLAMA_COMMON_TARGET llama-common)
else()
    set(NRVNA_LLAMA_COMMON_TARGET common)
endif()
target_include_directories(nrvna_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/vendor
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/common
)
target_link_libraries(nrvna_core PUBLIC
    llama
    mtmd
    ${NRVNA_LLAMA_COMMON_TARGET}
    Threads::Threads
)
target_compile_options(nrvna_core PRIVATE -Wall -Wextra -Wpedantic)

# CLI executables
add_executable(nrvnad cli/nrvnad.cpp)
target_link_libraries(nrvnad nrvna_core)

add_executable(wrk cli/wrk.cpp)
target_link_libraries(wrk nrvna_core)

add_executable(flw cli/flw.cpp)
target_link_libraries(flw nrvna_core)

foreach(_cli nrvnad wrk flw)
    target_compile_definitions(${_cli} PRIVATE NRVNA_VERSION="${PROJECT_VERSION}")
    target_compile_options(${_cli} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

if(BUILD_TESTING)
    set(NRVNA_TEST_BINS
        contract_test
        meta_test
        recovery_test
        crash_recovery_test
    )

    # This contract test uses headers and does not link llama.cpp.
    add_executable(contract_test tests/contract_test.cpp)
    target_include_directories(contract_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/vendor
    )

    add_executable(meta_test tests/meta_test.cpp src/meta.cpp)
    target_include_directories(meta_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_executable(recovery_test tests/recovery_test.cpp)
    target_link_libraries(recovery_test nrvna_core)
    target_include_directories(recovery_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_executable(crash_recovery_test tests/crash_recovery_test.cpp)
    target_link_libraries(crash_recovery_test nrvna_core)
    target_include_directories(crash_recovery_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_custom_target(nrvna_test_binaries DEPENDS ${NRVNA_TEST_BINS})

    add_test(NAME contract COMMAND contract_test)
    add_test(NAME metadata COMMAND meta_test)
    add_test(NAME recovery COMMAND recovery_test)
    add_test(NAME crash_recovery COMMAND crash_recovery_test)
    add_test(NAME primitive_cli COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/primitive-contract.sh $<TARGET_FILE_DIR:flw>)
    add_test(NAME lifecycle_cli COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/lifecycle-contract.sh $<TARGET_FILE_DIR:nrvnad>)
    add_test(NAME shell_helper COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/nrvna-lib-contract.sh ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME contract_literals COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/contract-literals.sh)
endif()

# Install targets
install(TARGETS nrvnad wrk flw
    RUNTIME DESTINATION bin
)
