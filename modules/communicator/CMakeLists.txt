set(PARACOOBA_COMMUNICATOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/service.cpp
)

parac_module(parac_communicator "${PARACOOBA_COMMUNICATOR_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
