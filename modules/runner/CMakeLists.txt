set(PARACOOBA_RUNNER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/runner.cpp
    )

parac_module(parac_runner "${PARACOOBA_RUNNER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
