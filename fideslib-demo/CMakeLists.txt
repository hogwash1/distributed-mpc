cmake_minimum_required(VERSION 3.25.2)

set(CMAKE_CXX_STANDARD 20)

project(eval_add_demo LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找已安装的 FIDESlib
find_package(fideslib REQUIRED CONFIG)

if (fideslib_FOUND)
    message(STATUS "FOUND PACKAGE FIDESlib")
    message(STATUS "FIDESlib Version: ${fideslib_VERSION}")
else ()
    message(FATAL_ERROR "PACKAGE FIDESlib NOT FOUND")
endif ()

set(CMAKE_CXX_COMPILER ${FIDESLIB_CXX_COMPILER})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FIDESLIB_CXX_FLAGS}")

# 可执行文件
add_executable(eval_add_demo eval_add_demo.cpp)
target_link_libraries(eval_add_demo PRIVATE fideslib::fideslib)
