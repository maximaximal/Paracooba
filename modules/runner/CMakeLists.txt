set(PARACOOBA_RUNNER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.cpp
    )

add_library(parac_runner SHARED ${PARACOOBA_RUNNER_SRCS})

generate_export_header(parac_runner)

target_link_libraries(parac_runner PUBLIC parac_common parac_modules)

target_include_directories(parac_runner PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
