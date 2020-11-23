set(PARACOOBA_SOLVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cadical_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cadical_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cube_source.cpp
    )

parac_module(parac_solver "${PARACOOBA_SOLVER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
target_link_libraries(parac_solver_obj PRIVATE libcadical cereal)
target_link_libraries(parac_solver_static PRIVATE libcadical)
target_link_libraries(parac_solver PRIVATE libcadical)
