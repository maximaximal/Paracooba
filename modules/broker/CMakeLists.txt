set(PARACOOBA_BROKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_task_store.cpp
    )

parac_module(parac_broker "${PARACOOBA_BROKER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)

target_link_libraries(parac_broker_obj PRIVATE cereal)

if(ENABLE_TESTS AND STATIC_BUILDS)
    add_library(parac_broker_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_broker_task_store.cpp
    )
    target_link_libraries(parac_broker_test PUBLIC parac_common Catch2::Catch2)

    target_link_libraries(parac_broker_test PRIVATE parac_broker_static)

    set(test_libs ${test_libs} parac_broker_test CACHE INTERNAL "Libraries for Tests")
endif()
