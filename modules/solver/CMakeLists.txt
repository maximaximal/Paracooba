set(PARACOOBA_SOLVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cadical_manager.cpp
    )

parac_module(parac_solver "${PARACOOBA_SOLVER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
