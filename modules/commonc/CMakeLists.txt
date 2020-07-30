add_library(parac_common SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/path.c
    ${CMAKE_CURRENT_SOURCE_DIR}/messaging_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/message_kind.c
)

target_link_libraries(parac_common parac_headers)

set_target_properties(parac_common PROPERTIES C_STANDARD 11)
