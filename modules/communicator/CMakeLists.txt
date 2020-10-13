set(PARACOOBA_COMMUNICATOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_sender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_acceptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_acceptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_connection_initiator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator_util.cpp
)

parac_module(parac_communicator "${PARACOOBA_COMMUNICATOR_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)
