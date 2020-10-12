set(PARACOOBA_BROKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/broker_compute_node_store.cpp
    )

parac_module(parac_broker "${PARACOOBA_BROKER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
