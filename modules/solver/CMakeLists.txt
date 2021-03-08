set(PARACOOBA_SOLVER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cadical_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cadical_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cube_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_config.cpp
    )

parac_module(parac_solver "${PARACOOBA_SOLVER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
target_link_libraries(parac_solver PRIVATE libcadical)

get_target_property(CEREAL_INCLUDE_DIRS cereal INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_solver_obj PRIVATE ${CEREAL_INCLUDE_DIRS})

get_target_property(CADICAL_INCLUDE_DIRS libcadical INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_solver_obj PRIVATE ${CADICAL_INCLUDE_DIRS})

add_dependencies(parac_solver_obj libcadical)

set_property(TARGET parac_solver_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES libcadical cereal)

if(STATIC_BUILDS)
    target_link_libraries(parac_solver_static PRIVATE libcadical)
endif()

if(ENABLE_TESTS)
    add_library(parac_solver_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_cube_source.cpp)

    if(${CMAKE_VERSION} VERSION_GREATER "3.11.0")
        target_link_libraries(parac_solver_test PRIVATE parac_solver_obj)
    else()
        target_link_libraries(parac_solver_test PRIVATE parac_solver)
    endif()

    target_link_libraries(parac_solver_test PRIVATE fmt cereal libcadical)

    target_link_libraries(parac_solver_test PUBLIC parac_common Catch2::Catch2 libcadical)

    target_include_directories(parac_solver_test PRIVATE ${Boost_INCLUDE_DIRS})

    target_compile_features(parac_solver_test PUBLIC cxx_std_17)

    set(test_libs ${test_libs} parac_solver_test CACHE INTERNAL "Libraries for Tests")
endif()
