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
    target_link_libraries(${suite} PRIVATE arena ctt::ctt)
    target_include_directories(${suite} PRIVATE
        ${CMAKE_SOURCE_DIR}/src   # platform.h for virtual_arena test
    )
    target_compile_options(${suite} PRIVATE
        $<$<NOT:$<BOOL:${MSVC}>>:-Wall -Wextra -Wpedantic>
        $<$<BOOL:${MSVC}>:/W4>
    )
    add_test(NAME ${suite} COMMAND ${suite})
endforeach()
