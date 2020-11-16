set(PARACOOBA_RUNNER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runner_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/runner_worker_executor.cpp
)

parac_module(parac_runner "${PARACOOBA_RUNNER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
