cmake_minimum_required(VERSION 3.15)
project(arena C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(arena STATIC
    src/platform.c
    src/fixed_arena.c
    src/growing_arena.c
    src/pool.c
    src/virtual_arena.c
    src/scratch.c
    src/stack_arena.c
    src/debug_allocator.c
)

target_include_directories(arena PUBLIC  include)
target_include_directories(arena PRIVATE src)

target_compile_options(arena PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<C_COMPILER_ID:MSVC>:/W4>
)

# ── Tests ────────────────────────────────────────────────────────────────────
option(BUILD_TESTING "Build test suite" ON)
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.16.0
        GIT_SHALLOW    TRUE
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)
endif()
