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
    he_evaluator.cpp
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
    expr_parser.cpp
    ${PROTO_SRCS}
)
target_include_directories(party_client PRIVATE ${ALL_INCLUDES})
target_link_directories(party_client PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(party_client PRIVATE ${ALL_LIBS})

# ============================================
# Test: AST Serializer (Task 01 — no OpenFHE/gRPC needed)
# ============================================
set(TEST_AST_SRCS
    test_ast_serializer.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/proto-generated/distributed_mpc.pb.cc"
)
add_executable(test_ast_serializer ${TEST_AST_SRCS})
target_include_directories(test_ast_serializer PRIVATE ${ALL_INCLUDES})
target_link_libraries(test_ast_serializer PRIVATE protobuf)

# ============================================
# Test: Expression Parser (Task 02 — no deps)
# ============================================
add_executable(test_expr_parser
    test_expr_parser.cpp
    expr_parser.cpp
)
target_include_directories(test_expr_parser PRIVATE ${ALL_INCLUDES})

# ============================================
# Test: Integration (Task 04 — OpenFHE needed, no gRPC)
# ============================================
add_executable(test_integration
    test_integration.cpp
    expr_parser.cpp
    he_evaluator.cpp
)
target_include_directories(test_integration PRIVATE ${ALL_INCLUDES})
target_link_directories(test_integration PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(test_integration PRIVATE ${OpenFHE_LIBRARIES})
