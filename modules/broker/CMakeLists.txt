set(PARACOOBA_BROKER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/broker.cpp
    )

add_library(parac_broker SHARED ${PARACOOBA_BROKER_SRCS})

generate_export_header(parac_broker)

target_link_libraries(parac_broker PUBLIC parac_common parac_modules)

target_include_directories(parac_broker PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
