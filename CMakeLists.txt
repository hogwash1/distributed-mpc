cmake_minimum_required(VERSION 3.25.2)

set(CMAKE_CXX_STANDARD 20)

project(distributed_mpc LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find OpenFHE
find_package(OpenFHE 1.4.2 REQUIRED CONFIG)

if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
else ()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif ()

# Find OpenSSL for SHA256
find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    message(STATUS "FOUND OpenSSL: ${OPENSSL_VERSION}")
endif ()

# Proto generated sources
set(PROTO_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/proto-generated/distributed_mpc.pb.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/proto-generated/distributed_mpc.grpc.pb.cc"
)

# Proto include directories
set(PROTO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto-generated")

# OpenFHE include directories
set(OPENFHE_INCLUDE_DIR "${OpenFHE_INCLUDE}")
set(OPENFHE_PKE_DIR "${OpenFHE_INCLUDE}/pke")
set(OPENFHE_CORE_DIR "${OpenFHE_INCLUDE}/core")
set(OPENFHE_BINFHE_DIR "${OpenFHE_INCLUDE}/binfhe")

# Local include directories (current directory for headers)
set(LOCAL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# All includes
set(ALL_INCLUDES
    "${PROTO_INCLUDE_DIR}"
    "${OPENFHE_INCLUDE_DIR}"
    "${OPENFHE_PKE_DIR}"
    "${OPENFHE_CORE_DIR}"
    "${OPENFHE_BINFHE_DIR}"
    "${LOCAL_INCLUDE_DIR}"
)

# All libs
set(ALL_LIBS
    "${OpenFHE_LIBRARIES}"
    OpenSSL::SSL OpenSSL::Crypto
    grpc++
    protobuf
)

# ============================================
# Compute Server
# ============================================
add_executable(compute_server
    compute_server.h
    compute_server.cpp
    compute_server_main.cpp
    ${PROTO_SRCS}
)
target_include_directories(compute_server PRIVATE ${ALL_INCLUDES})
target_link_directories(compute_server PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(compute_server PRIVATE ${ALL_LIBS})

# ============================================
# Party Client
# ============================================
add_executable(party_client
    party_client.h
    party_client.cpp
    party_client_main.cpp
    grpc_serializer.h
    ${PROTO_SRCS}
)
target_include_directories(party_client PRIVATE ${ALL_INCLUDES})
target_link_directories(party_client PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(party_client PRIVATE ${ALL_LIBS})
