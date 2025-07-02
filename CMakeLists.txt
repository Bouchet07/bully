cmake_minimum_required(VERSION 3.16)

# Name of your project
project(bully)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add all .cpp files from the src directory
file(GLOB SOURCES "src/*.cpp")

# Create the executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include header files
target_include_directories(${PROJECT_NAME} PRIVATE src)

# UTF-8 support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

# Test
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
  )
  FetchContent_MakeAvailable(googletest)
  enable_testing()
  file(GLOB TEST_SOURCES "tests/*.cpp") # Add this line
  add_executable(tests ${TEST_SOURCES}) # Use all test sources
  target_link_libraries(tests gtest_main)
  add_test(NAME AllTests COMMAND tests)
endif()