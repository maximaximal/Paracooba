set(PARACOOBA_BROKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    )

parac_module(parac_broker "${PARACOOBA_BROKER_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
