set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/paracooba_test_main.cpp
)

add_executable(parac_test ${TEST_SRCS})

set_property(TARGET parac_test PROPERTY CXX_STANDARD 17)

target_link_libraries(parac_test Catch2::Catch2)

target_link_libraries(parac_test ${test_libs})

catch_discover_tests(parac_test)
