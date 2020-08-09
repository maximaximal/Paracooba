set(PARACOOBA_SOLVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver.cpp
    )

add_library(parac_solver SHARED ${PARACOOBA_SOLVER_SRCS})

generate_export_header(parac_solver)

target_link_libraries(parac_solver PUBLIC parac_common parac_modules)

target_include_directories(parac_solver PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
