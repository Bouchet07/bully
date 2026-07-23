cmake_minimum_required(VERSION 3.20)
project(Bully C CXX)

# Require C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IPO/LTO configuration for release builds (Link-Time Optimization)
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
if(ipo_supported)
    message(STATUS "IPO/LTO enabled for Release builds")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
else()
    message(STATUS "IPO/LTO not supported: ${ipo_output}")
endif()

# Define core engine source files (excluding main.cpp)
set(ENGINE_SOURCES
    src/bitboard.cpp
    src/attacks.cpp
    src/position.cpp
    src/movegen.cpp
    src/tt.cpp
    src/search.cpp
    src/evaluation.cpp
    src/uci.cpp
    src/syzygy.cpp
    src/cli.cpp
    Pyrrhic/tbprobe.c
)

set(ENGINE_HEADERS
    src/types.h
    src/bitboard.h
    src/attacks.h
    src/position.h
    src/movegen.h
    src/tt.h
    src/search.h
    src/evaluation.h
    src/uci.h
    src/syzygy.h
    src/cli.h
)

# Define static library for the engine to share with the executable and tests
add_library(engine_lib STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
set_source_files_properties(Pyrrhic/tbprobe.c PROPERTIES 
    LANGUAGE CXX
    COMPILE_FLAGS "-w"
)
target_include_directories(engine_lib PUBLIC src Pyrrhic)

# ============================================================================
# Professional Build Architecture Configurations
# ============================================================================
set(BULLY_ARCH "native" CACHE STRING "Target CPU architecture (generic, modern, avx2, bmi2, native)")
set_property(CACHE BULLY_ARCH PROPERTY STRINGS "generic" "modern" "avx2" "bmi2" "native")

# Translate architecture selection to compiler flags and binary suffix
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Base warning and optimization flags
    set(ARCH_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -O3
    )

    if(BULLY_ARCH STREQUAL "native")
        list(APPEND ARCH_FLAGS -march=native)
        set(BINARY_SUFFIX "-native")
    elseif(BULLY_ARCH STREQUAL "bmi2")
        list(APPEND ARCH_FLAGS -march=x86-64-v3 -mbmi2) # x86-64-v3 requires AVX2/BMI2
        set(BINARY_SUFFIX "-bmi2")
    elseif(BULLY_ARCH STREQUAL "avx2")
        list(APPEND ARCH_FLAGS -march=x86-64-v3 -mno-bmi2) # Has AVX2 but disables BMI2 (forces magic fallback)
        set(BINARY_SUFFIX "-avx2")
    elseif(BULLY_ARCH STREQUAL "modern")
        list(APPEND ARCH_FLAGS -march=x86-64-v2) # SSE4.2 + POPCNT
        set(BINARY_SUFFIX "-modern")
    else()
        # generic / standard x86-64 (v1)
        list(APPEND ARCH_FLAGS -march=x86-64)
        set(BINARY_SUFFIX "")
    endif()

    target_compile_options(engine_lib PUBLIC ${ARCH_FLAGS})

    # If BMI2 was enabled, ensure public dependency compiles with -mbmi2 and defines USE_PEXT
    if(BULLY_ARCH STREQUAL "bmi2")
        target_compile_options(engine_lib PUBLIC -mbmi2)
        target_compile_definitions(engine_lib PUBLIC USE_PEXT)
    endif()

elseif(MSVC)
    set(ARCH_FLAGS
        /W4
        /utf-8
        /O2
        /Ob2
        /Oi
        /Ot
    )

    if(BULLY_ARCH STREQUAL "native" OR BULLY_ARCH STREQUAL "bmi2")
        list(APPEND ARCH_FLAGS /arch:AVX2) # On MSVC, AVX2 targets BMI2
        target_compile_definitions(engine_lib PUBLIC USE_PEXT)
        set(BINARY_SUFFIX "-bmi2")
    elseif(BULLY_ARCH STREQUAL "avx2")
        list(APPEND ARCH_FLAGS /arch:AVX2)
        set(BINARY_SUFFIX "-avx2")
    elseif(BULLY_ARCH STREQUAL "modern")
        set(BINARY_SUFFIX "-modern")
    else()
        set(BINARY_SUFFIX "")
    endif()

    target_compile_options(engine_lib PUBLIC ${ARCH_FLAGS})
endif()

# Create the main executable
add_executable(bully src/main.cpp)
target_link_libraries(bully PRIVATE engine_lib)

# Automatically rename binary based on target architecture
set_target_properties(bully PROPERTIES OUTPUT_NAME "bully${BINARY_SUFFIX}")

# ============================================================================
# Test Configuration
# ============================================================================
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )
    # Prevent GoogleTest from overriding compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    enable_testing()
    
    set(TEST_SOURCES
        tests/test_bitboard.cpp
        tests/test_types.cpp
        tests/test_position.cpp
        tests/test_movegen.cpp
        tests/test_tt.cpp
        tests/test_evaluation.cpp
    )
    
    add_executable(tests ${TEST_SOURCES})
    target_link_libraries(tests PRIVATE engine_lib gtest_main)
    
    add_test(NAME AllTests COMMAND tests)
endif()