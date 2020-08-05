add_library(parac_common SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/path.c
    ${CMAKE_CURRENT_SOURCE_DIR}/message.c
    ${CMAKE_CURRENT_SOURCE_DIR}/message_kind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/task_store_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compute_node_store_adapter.c
)

target_link_libraries(parac_common parac_headers)

set_target_properties(parac_common PROPERTIES C_STANDARD 11)

generate_export_header(parac_common)
target_include_directories(parac_common PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(ENABLE_TESTS)
    add_library(parac_common_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_path.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_task_store_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_compute_node_store_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_config.cpp
    )
    target_link_libraries(parac_common_test PUBLIC parac_common Catch2::Catch2)

    set(test_libs ${test_libs} parac_common_test CACHE INTERNAL "Libraries for Tests")
endif()
