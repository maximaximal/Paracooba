set(TEST_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/paracooba_test_main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/../loader/StaticModuleLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_connect_two_daemons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_connect_daemon_and_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_task_store.cpp
)

add_executable(parac_test ${TEST_SRCS})

target_compile_features(parac_test PUBLIC cxx_std_17)

target_link_libraries(parac_test Catch2::Catch2)

target_link_libraries(parac_test ${test_libs})

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(parac_test "-lstdthreads")
endif()

target_link_libraries(
    parac_test
    parac_modules
    parac_loader
    Boost::filesystem Boost::log
    fmt
    libcadical
    libkissat)

catch_discover_tests(parac_test)
