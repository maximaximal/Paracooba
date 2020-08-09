set(PARACOOBA_COMMUNICATOR_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/communicator.cpp
    )

add_library(parac_communicator SHARED ${PARACOOBA_COMMUNICATOR_SRCS})

generate_export_header(parac_communicator)

target_link_libraries(parac_communicator PUBLIC parac_common parac_modules)

target_include_directories(parac_communicator PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
