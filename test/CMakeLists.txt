set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/paracooba_test_main.cpp
)

set(INTEGRATION_TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../loader/StaticModuleLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_connect_two_daemons.cpp
)

if(STATIC_BUILDS)
    set(TEST_SRCS ${TEST_SRCS} ${INTEGRATION_TEST_SRCS})
else()
    message(STATUS "Not activating integration tests when static library builds are disabled!")
endif()

add_executable(parac_test ${TEST_SRCS})

set_property(TARGET parac_test PROPERTY CXX_STANDARD 17)

target_link_libraries(parac_test Catch2::Catch2)

target_link_libraries(parac_test ${test_libs})

if(STATIC_BUILDS)
    # Integration tests need to be linked to all
    # other paracooba modules, so testing code is
    # linked to all other modules.
    target_link_libraries(
        parac_test
        parac_modules
        parac_loader
        Boost::filesystem Boost::log
        fmt)
endif()

catch_discover_tests(parac_test)
