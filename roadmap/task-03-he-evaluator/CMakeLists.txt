cmake_minimum_required(VERSION 3.25.2)
set(CMAKE_CXX_STANDARD 20)
project(task_03_he_evaluator LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find OpenFHE (no gRPC/OpenSSL needed for unit tests)
find_package(OpenFHE 1.4.2 REQUIRED CONFIG)

if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
else()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif()

# Source files for the test executable
set(TEST_SOURCES
    test_he_evaluator.cpp
    he_evaluator.cpp
)

# Include directories
set(ALL_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${OpenFHE_INCLUDE}"
    "${OpenFHE_INCLUDE}/pke"
    "${OpenFHE_INCLUDE}/core"
    "${OpenFHE_INCLUDE}/binfhe"
)

# Build test executable
add_executable(test_he_evaluator ${TEST_SOURCES})
target_include_directories(test_he_evaluator PRIVATE ${ALL_INCLUDES})
target_link_directories(test_he_evaluator PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(test_he_evaluator PRIVATE ${OpenFHE_LIBRARIES})
