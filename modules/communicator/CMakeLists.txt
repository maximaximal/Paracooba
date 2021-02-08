set(PARACOOBA_COMMUNICATOR_SRCS
    ${CMAKE_BINARY_DIR}/distrac_paracooba.h

    ${CMAKE_CURRENT_SOURCE_DIR}/communicator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_sender.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/udp_acceptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_acceptor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp_connection_initiator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/timeout_controller.cpp
)

parac_module(parac_communicator "${PARACOOBA_COMMUNICATOR_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)

set_source_files_properties(${CMAKE_BINARY_DIR}/distrac_paracooba.h PROPERTIES GENERATED 1)
add_dependencies(parac_communicator_obj distrac_paracooba)

set_property(TARGET parac_solver_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES distrac)
