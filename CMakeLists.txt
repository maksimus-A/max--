cmake_minimum_required(VERSION 3.20)
project(maxc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Default to Debug if not specified
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---- Options ----
option(MAXC_ARENA_TESTS "Compile arena test code into the binary" OFF)
option(MAXC_SANITIZE "Enable extra warnings + ASan/UBSan" OFF)
option(MAXC_EXPORT_COMPILE_COMMANDS "Export compile_commands.json" OFF)
option(MAXC_ARENA_DUMP "Enable arena memory dumps during arena tests" OFF)


set(CMAKE_EXPORT_COMPILE_COMMANDS ${MAXC_EXPORT_COMPILE_COMMANDS})

# ---- Sources ----
set(MAXC_SOURCES
    src/main.c
    src/ast/lexer/lexer.c
    src/ast/parser/parser.c
    src/ast/parser/ast_printer.c
    src/semantics/scope.c
    src/semantics/walker.c
    src/semantics/def-assn-analysis/def_assn.c
    src/table/ptrtable.c
    src/errors/diagnostics.c
    src/debug.c
    src/common.c
    src/arena/arena.c
)

if (MAXC_ARENA_TESTS)
    list(APPEND MAXC_SOURCES src/arena/arena_test.c)
endif()

add_executable(maxc ${MAXC_SOURCES})

target_include_directories(maxc PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Let the code know arena tests are available
if (MAXC_ARENA_TESTS)
    target_compile_definitions(maxc PRIVATE MAXC_ARENA_TESTS=1)
endif()

if (MAXC_ARENA_DUMP)
    target_compile_definitions(maxc PRIVATE MAXC_ARENA_DUMP=1)
endif()

# ---- Sanitizers / warnings ----
if (MAXC_SANITIZE)
    target_compile_options(maxc PRIVATE
        -g -O0
        -Wall -Wextra -Wpedantic
    )

    if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(maxc PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(maxc PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endif()
