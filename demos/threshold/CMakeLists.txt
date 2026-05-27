cmake_minimum_required(VERSION 3.25.2)

set(CMAKE_CXX_STANDARD 20)

project(threshold_demo LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find OpenFHE directly
find_package(OpenFHE 1.4.2 REQUIRED CONFIG)

if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
    message(STATUS "OpenFHE Version: ${OpenFHE_VERSION}")
    message(STATUS "OpenFHE include files location: ${OpenFHE_INCLUDE}")
else ()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif ()

# Executable
add_executable(threshold_demo threshold_demo.cpp)

# Include directories - correct paths
target_include_directories(threshold_demo PRIVATE ${OpenFHE_INCLUDE})
target_include_directories(threshold_demo PRIVATE ${OpenFHE_INCLUDE}/pke)
target_include_directories(threshold_demo PRIVATE ${OpenFHE_INCLUDE}/core)
target_include_directories(threshold_demo PRIVATE ${OpenFHE_INCLUDE}/binfhe)

# Link directories
target_link_directories(threshold_demo PRIVATE ${OpenFHE_LIBDIR})

# Link libraries
target_link_libraries(threshold_demo PRIVATE ${OpenFHE_LIBRARIES})
