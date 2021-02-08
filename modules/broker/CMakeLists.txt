set(PARACOOBA_BROKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_task_store.cpp
    )

parac_module(parac_broker "${PARACOOBA_BROKER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)

set_property(TARGET parac_broker_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES cereal)

get_target_property(CEREAL_INCLUDE_DIRS cereal INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_broker_obj PRIVATE ${CEREAL_INCLUDE_DIRS})
