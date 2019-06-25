set(CAPNP_SCHEMAS
    ${CMAKE_CURRENT_SOURCE_DIR}/message.capnp
    )

capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS ${CAPNP_SCHEMAS})

add_library(capnp_schemas_lib OBJECT ${CAPNP_SRCS})
