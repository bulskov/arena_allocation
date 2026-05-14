cmake_minimum_required(VERSION 3.15)
project(arena C)

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
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(CRITERION criterion)
    endif()

    if(NOT CRITERION_FOUND)
        find_program(MESON_EXE meson)
        if(MESON_EXE)
            message(STATUS "Criterion not found — fetching v2.4.2 from GitHub (Meson build)…")
            include(ExternalProject)
            set(_crit_root "${CMAKE_BINARY_DIR}/_criterion")
            set(_crit_lib
                "${_crit_root}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}criterion${CMAKE_SHARED_LIBRARY_SUFFIX}")
            ExternalProject_Add(criterion_fetch
                GIT_REPOSITORY  https://github.com/Snaipe/Criterion.git
                GIT_TAG         v2.4.2
                GIT_SHALLOW     TRUE
                CONFIGURE_COMMAND
                    ${MESON_EXE} setup
                        --buildtype=release
                        --prefix=<INSTALL_DIR>
                        --libdir=lib
                        <BINARY_DIR> <SOURCE_DIR>
                BUILD_COMMAND   ${MESON_EXE} compile -C <BINARY_DIR>
                INSTALL_COMMAND ${MESON_EXE} install -C <BINARY_DIR>
                INSTALL_DIR     "${_crit_root}"
                BUILD_BYPRODUCTS "${_crit_lib}"
            )
            add_library(criterion_fetched SHARED IMPORTED GLOBAL)
            add_dependencies(criterion_fetched criterion_fetch)
            set_target_properties(criterion_fetched PROPERTIES
                IMPORTED_LOCATION "${_crit_lib}"
                INTERFACE_INCLUDE_DIRECTORIES "${_crit_root}/include"
            )
            # tests need to find the shared lib at ctest time
            list(APPEND CMAKE_BUILD_RPATH "${_crit_root}/lib")
            set(CRITERION_LIBRARIES   criterion_fetched)
            set(CRITERION_INCLUDE_DIRS "")
            set(CRITERION_CFLAGS_OTHER "")
            set(CRITERION_FOUND TRUE)
        else()
            message(STATUS "Criterion not found — skipping tests.\n"
                "Install:  apt install libcriterion-dev  |  brew install criterion\n"
                "Or install Meson (pip install meson ninja) to have CMake fetch it automatically.")
        endif()
    endif()

    if(CRITERION_FOUND)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
