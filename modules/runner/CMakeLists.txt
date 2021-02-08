set(PARACOOBA_RUNNER_SRCS
    ${CMAKE_BINARY_DIR}/distrac_paracooba.h

    ${CMAKE_CURRENT_SOURCE_DIR}/runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runner_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runner_worker_executor.cpp
)

parac_module(parac_runner "${PARACOOBA_RUNNER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)

set_source_files_properties(${CMAKE_BINARY_DIR}/distrac_paracooba.h PROPERTIES GENERATED 1)
add_dependencies(parac_runner_obj distrac_paracooba)

set_property(TARGET parac_runner_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES distrac)
