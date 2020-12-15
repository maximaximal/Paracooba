set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/paracooba_test_main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/../loader/StaticModuleLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_connect_two_daemons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_connect_daemon_and_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_task_store.cpp
)

add_executable(parac_test ${TEST_SRCS})

set_property(TARGET parac_test PROPERTY CXX_STANDARD 17)

target_link_libraries(parac_test Catch2::Catch2)

target_link_libraries(parac_test ${test_libs})

target_link_libraries(
    parac_test
    parac_modules
    parac_loader
    Boost::filesystem Boost::log
    fmt)

catch_discover_tests(parac_test)
