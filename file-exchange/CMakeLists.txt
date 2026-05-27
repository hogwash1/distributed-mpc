cmake_minimum_required(VERSION 3.25.2)

set(CMAKE_CXX_STANDARD 20)

project(three_party_file LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find OpenFHE
find_package(OpenFHE 1.4.2 REQUIRED CONFIG)

if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
else ()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif ()

# Party A executable
add_executable(party_a_file party_a_file.cpp)
target_link_libraries(party_a_file PRIVATE ${OpenFHE_LIBRARIES})
target_include_directories(party_a_file PRIVATE ${OpenFHE_INCLUDE})
target_include_directories(party_a_file PRIVATE ${OpenFHE_INCLUDE}/pke)
target_include_directories(party_a_file PRIVATE ${OpenFHE_INCLUDE}/core)
target_include_directories(party_a_file PRIVATE ${OpenFHE_INCLUDE}/binfhe)

# Party B executable
add_executable(party_b_file party_b_file.cpp)
target_link_libraries(party_b_file PRIVATE ${OpenFHE_LIBRARIES})
target_include_directories(party_b_file PRIVATE ${OpenFHE_INCLUDE})
target_include_directories(party_b_file PRIVATE ${OpenFHE_INCLUDE}/pke)
target_include_directories(party_b_file PRIVATE ${OpenFHE_INCLUDE}/core)
target_include_directories(party_b_file PRIVATE ${OpenFHE_INCLUDE}/binfhe)

# Party C executable
add_executable(party_c_file party_c_file.cpp)
target_link_libraries(party_c_file PRIVATE ${OpenFHE_LIBRARIES})
target_include_directories(party_c_file PRIVATE ${OpenFHE_INCLUDE})
target_include_directories(party_c_file PRIVATE ${OpenFHE_INCLUDE}/pke)
target_include_directories(party_c_file PRIVATE ${OpenFHE_INCLUDE}/core)
target_include_directories(party_c_file PRIVATE ${OpenFHE_INCLUDE}/binfhe)
