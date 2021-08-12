set(PARACOOBA_SOLVER_QBF_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_qbf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/solver_qbf_config.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/generic_qbf_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/depqbf_handle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser_qbf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qbf_parser_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qbf_solver_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qbf_solver_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qbf_cube_source.cpp
    )

parac_module(parac_solver_qbf "${PARACOOBA_SOLVER_QBF_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
target_link_libraries(parac_solver_qbf PRIVATE libdepqbf)

get_target_property(CEREAL_INCLUDE_DIRS cereal INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_solver_qbf_obj PRIVATE ${CEREAL_INCLUDE_DIRS})

get_target_property(DEPQBF_INCLUDE_DIRS libdepqbf INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_solver_qbf_obj PRIVATE ${DEPQBF_INCLUDE_DIRS})
add_dependencies(parac_solver_qbf_obj libdepqbf)

set_property(TARGET parac_solver_qbf_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES cereal)

if(STATIC_BUILDS)
    target_link_libraries(parac_solver_qbf_static PRIVATE libdepqbf)
endif()

if(ENABLE_TESTS)
    add_library(parac_solver_qbf_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_qbf_parser.cpp
        )

    if(${CMAKE_VERSION} VERSION_GREATER "3.11.0")
        target_link_libraries(parac_solver_qbf_test PRIVATE parac_solver_qbf_obj)
    else()
        target_link_libraries(parac_solver_qbf_test PRIVATE parac_solver_qbf)
    endif()

    target_link_libraries(parac_solver_qbf_test PRIVATE fmt)

    target_link_libraries(parac_solver_qbf_test PUBLIC parac_common Catch2::Catch2 libdepqbf)

    target_include_directories(parac_solver_qbf_test PRIVATE ${Boost_INCLUDE_DIRS})

    target_compile_features(parac_solver_qbf_test PUBLIC cxx_std_17)

    set(test_libs ${test_libs} parac_solver_qbf_test CACHE INTERNAL "Libraries for Tests")
endif()
