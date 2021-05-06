set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

add_subdirectory(commonc)

add_library(parac_modules STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/module.c
    )

generate_export_header(parac_modules)
target_include_directories(parac_modules PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(parac_modules parac_headers)

add_subdirectory(runner)
add_subdirectory(broker)
add_subdirectory(solver)
add_subdirectory(solver_qbf)
add_subdirectory(communicator)
