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
    src/movepicker.cpp
    src/tt.cpp
    src/search.cpp
    src/evaluation.cpp
    src/nnue.cpp
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
    src/movepicker.h
    src/tt.h
    src/search.h
    src/evaluation.h
    src/nnue.h
    src/uci.h
    src/syzygy.h
    src/cli.h
)

# Copy customized tablebase config to the submodule directory at configure time
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tbconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Pyrrhic/tbconfig.h
    COPYONLY
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
set(BULLY_ARCH "native" CACHE STRING "Target CPU architecture (generic, modern, avx2, bmi2, avx512, avx512-vnni, armv8, armv8-dotprod, native)")
set_property(CACHE BULLY_ARCH PROPERTY STRINGS "generic" "modern" "avx2" "bmi2" "avx512" "avx512-vnni" "armv8" "armv8-dotprod" "native")

# Determine if target is ARM (Android, ARM64 Apple Silicon, ARM Linux)
if(ANDROID OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64|AARCH64" OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(IS_ARM_TARGET TRUE)
else()
    set(IS_ARM_TARGET FALSE)
endif()

# Default to Release build type if not explicitly set
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose build type (Release, Debug)" FORCE)
endif()

# Translate architecture selection to compiler flags and binary suffix
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Base warning flags
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
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Configuring DEBUG build (-O1 -g -fno-omit-frame-pointer)")
        list(APPEND ARCH_FLAGS -O1 -g -fno-omit-frame-pointer)
        set(DEBUG_SUFFIX "-debug")
    else()
        message(STATUS "Configuring RELEASE build (-O3)")
        list(APPEND ARCH_FLAGS -O3)
        set(DEBUG_SUFFIX "")
    endif()

    if(IS_ARM_TARGET)
        if(BULLY_ARCH STREQUAL "armv8-dotprod")
            list(APPEND ARCH_FLAGS -march=armv8.2-a+dotprod)
            target_compile_definitions(engine_lib PUBLIC USE_DOTPROD)
            set(BINARY_SUFFIX "-armv8-dotprod")
        elseif(BULLY_ARCH STREQUAL "armv8")
            list(APPEND ARCH_FLAGS -march=armv8-a)
            set(BINARY_SUFFIX "-armv8")
        else()
            # For Android or generic ARM targets, do not pass x86 -march flags
            if(ANDROID)
                list(APPEND ARCH_FLAGS -march=armv8-a)
            endif()
            set(BINARY_SUFFIX "")
        endif()
    else()
        # x86-64 target architectures
        if(BULLY_ARCH STREQUAL "native")
            list(APPEND ARCH_FLAGS -march=native)
            target_compile_definitions(engine_lib PUBLIC USE_PEXT)
            set(BINARY_SUFFIX "-native")
        elseif(BULLY_ARCH STREQUAL "avx512-vnni")
            list(APPEND ARCH_FLAGS -march=x86-64-v4 -mbmi2 -mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni)
            target_compile_definitions(engine_lib PUBLIC USE_PEXT USE_VNNI)
            set(BINARY_SUFFIX "-avx512-vnni")
        elseif(BULLY_ARCH STREQUAL "avx512")
            list(APPEND ARCH_FLAGS -march=x86-64-v4 -mbmi2 -mavx512f -mavx512bw -mavx512dq -mavx512vl)
            target_compile_definitions(engine_lib PUBLIC USE_PEXT)
            set(BINARY_SUFFIX "-avx512")
        elseif(BULLY_ARCH STREQUAL "bmi2")
            list(APPEND ARCH_FLAGS -march=x86-64-v3 -mbmi2) # x86-64-v3 requires AVX2/BMI2
            target_compile_definitions(engine_lib PUBLIC USE_PEXT)
            set(BINARY_SUFFIX "-bmi2")
        elseif(BULLY_ARCH STREQUAL "avx2")
            list(APPEND ARCH_FLAGS -march=x86-64-v3 -mno-bmi2) # Has AVX2 but disables BMI2
            set(BINARY_SUFFIX "-avx2")
        elseif(BULLY_ARCH STREQUAL "modern")
            list(APPEND ARCH_FLAGS -march=x86-64-v2) # SSE4.2 + POPCNT
            set(BINARY_SUFFIX "-modern")
        else()
            # generic / standard baseline x86-64
            list(APPEND ARCH_FLAGS -march=x86-64)
            set(BINARY_SUFFIX "")
        endif()
    endif()

    target_compile_options(engine_lib PUBLIC ${ARCH_FLAGS})

elseif(MSVC)
    add_compile_definitions(NOMINMAX)
    add_compile_options(/wd4324)
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

string(JOIN " " ARCH_FLAGS_JOINED ${ARCH_FLAGS})
target_compile_definitions(engine_lib PUBLIC
    COMPILER_NAME="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    BUILD_ARCH_NAME="${BULLY_ARCH}"
    BUILD_FLAGS_STR="${ARCH_FLAGS_JOINED}"
)

# Create the main executable
add_executable(bully src/main.cpp)
target_link_libraries(bully PRIVATE engine_lib)

# Automatically rename binary based on target architecture and build type
set_target_properties(bully PROPERTIES OUTPUT_NAME "bully${BINARY_SUFFIX}${DEBUG_SUFFIX}")

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
        tests/test_nnue.cpp
        tests/test_search.cpp
        tests/test_syzygy.cpp
        tests/test_uci.cpp
    )
    
    add_executable(tests ${TEST_SOURCES})
    target_link_libraries(tests PRIVATE engine_lib gtest_main)
    
    include(GoogleTest)
    gtest_discover_tests(tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endif()