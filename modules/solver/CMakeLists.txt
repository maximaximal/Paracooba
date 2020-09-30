set(PARACOOBA_SOLVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver.cpp
    )

parac_module(parac_solver "${PARACOOBA_SOLVER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
