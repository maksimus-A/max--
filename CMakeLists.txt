cmake_minimum_required(VERSION 3.20)
project(maxc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(maxc
    src/main.c
    src/ast/lexer/lexer.c
    src/debug.c
    src/common.c
    src/ast/ast_node.c
)

target_include_directories(maxc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
