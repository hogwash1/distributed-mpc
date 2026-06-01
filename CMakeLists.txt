cmake_minimum_required(VERSION 3.25.2)
set(CMAKE_CXX_STANDARD 20)
project(MPC_Demo LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找 OpenFHE
find_package(OpenFHE 1.4.2 REQUIRED CONFIG)

# 头文件路径 — 所有任务文件夹
set(ALL_INCLUDES
    "${OpenFHE_INCLUDE}"
    "${OpenFHE_INCLUDE}/pke"
    "${OpenFHE_INCLUDE}/core"
    "${OpenFHE_INCLUDE}/binfhe"
    "${CMAKE_CURRENT_SOURCE_DIR}/task-01-ast"
    "${CMAKE_CURRENT_SOURCE_DIR}/task-02-parser"
    "${CMAKE_CURRENT_SOURCE_DIR}/task-03-evaluator"
    "${CMAKE_CURRENT_SOURCE_DIR}/task-04-integration"
    "${CMAKE_CURRENT_SOURCE_DIR}/task-05-multiparty"
)

# ============================================
# test_expr_parser — 表达式解析器 (零依赖)
# ============================================
add_executable(test_expr_parser
    task-02-parser/test_expr_parser.cpp
    task-02-parser/expr_parser.cpp
)
target_include_directories(test_expr_parser PRIVATE ${ALL_INCLUDES})

# ============================================
# test_integration — 集成流水线 (依赖 OpenFHE)
# ============================================
add_executable(test_integration
    task-04-integration/test_integration.cpp
    task-02-parser/expr_parser.cpp
    task-03-evaluator/he_evaluator.cpp
)
target_include_directories(test_integration PRIVATE ${ALL_INCLUDES})
target_link_directories(test_integration PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(test_integration PRIVATE ${OpenFHE_LIBRARIES})

# ============================================
# test_multiparty — 多方测试
# ============================================
add_executable(test_multiparty
    task-05-multiparty/test_multiparty.cpp
    task-02-parser/expr_parser.cpp
    task-03-evaluator/he_evaluator.cpp
)
target_include_directories(test_multiparty PRIVATE ${ALL_INCLUDES})
target_link_directories(test_multiparty PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(test_multiparty PRIVATE ${OpenFHE_LIBRARIES})

# ============================================
# mpc_demo — 单进程多方安全计算 Demo
# ============================================
add_executable(mpc_demo
    task-05-multiparty/mpc_demo.cpp
    task-02-parser/expr_parser.cpp
    task-03-evaluator/he_evaluator.cpp
)
target_include_directories(mpc_demo PRIVATE ${ALL_INCLUDES})
target_link_directories(mpc_demo PRIVATE ${OpenFHE_LIBDIR})
target_link_libraries(mpc_demo PRIVATE ${OpenFHE_LIBRARIES})
