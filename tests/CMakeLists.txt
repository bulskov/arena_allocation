include(GoogleTest)

set(TEST_SUITES
    test_fixed_arena
    test_growing_arena
    test_pool
    test_virtual_arena
    test_scratch
    test_stack_arena
    test_debug_allocator
)

foreach(suite ${TEST_SUITES})
    add_executable(${suite} ${suite}.c)
    set_source_files_properties(${suite}.c PROPERTIES LANGUAGE CXX)
    target_link_libraries(${suite} PRIVATE arena GTest::gtest_main)
    target_include_directories(${suite} PRIVATE
        ${CMAKE_SOURCE_DIR}/src   # platform.h for virtual_arena test
    )
    set_target_properties(${suite} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
    gtest_discover_tests(${suite})
endforeach()
