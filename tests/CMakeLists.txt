set(TEST_SUITES
    test_fixed_arena
    test_growing_arena
    test_pool
    test_virtual_arena
    test_scratch
)

foreach(suite ${TEST_SUITES})
    add_executable(${suite} ${suite}.c)
    target_link_libraries(${suite} PRIVATE arena ${CRITERION_LIBRARIES})
    target_include_directories(${suite} PRIVATE
        ${CRITERION_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src   # platform.h for virtual_arena test
    )
    target_compile_options(${suite} PRIVATE ${CRITERION_CFLAGS_OTHER})
    add_test(NAME ${suite} COMMAND ${suite})
endforeach()
