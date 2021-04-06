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
    ${CMAKE_CURRENT_SOURCE_DIR}/message_send_queue.cpp
)

parac_module(parac_communicator "${PARACOOBA_COMMUNICATOR_SRCS}" ${CMAKE_CURRENT_SOURCE_DIR}/discover.c)

if(ENABLE_DISTRAC)
    set_source_files_properties(${CMAKE_BINARY_DIR}/distrac_paracooba.h PROPERTIES GENERATED 1)
    add_dependencies(parac_communicator_obj distrac_paracooba)
endif()

get_target_property(CEREAL_INCLUDE_DIRS cereal INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(parac_communicator_obj PRIVATE ${CEREAL_INCLUDE_DIRS})

target_include_directories(parac_communicator_obj PRIVATE ${BOOST_INCLUDE_DIRECTORIES})

set_property(TARGET parac_solver_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES distrac)

if(ENABLE_TESTS)
    add_library(parac_communicator_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_message_send_queue.cpp)

    if(${CMAKE_VERSION} VERSION_GREATER "3.11.0")
        target_link_libraries(parac_communicator_test PRIVATE parac_communicator_obj)
    else()
        target_link_libraries(parac_communicator_test PRIVATE parac_communicator)
    endif()

    target_link_libraries(parac_communicator_test PRIVATE fmt cereal)

    target_link_libraries(parac_communicator_test PUBLIC parac_common Catch2::Catch2)

    target_include_directories(parac_communicator_test PRIVATE ${Boost_INCLUDE_DIRS})

    target_compile_features(parac_communicator_test PUBLIC cxx_std_17)

    set(test_libs ${test_libs} parac_communicator_test CACHE INTERNAL "Libraries for Tests")
endif()
