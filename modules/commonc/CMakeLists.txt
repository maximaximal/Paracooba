set(PARAC_COMMON_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/path.c
    ${CMAKE_CURRENT_SOURCE_DIR}/file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/message.c
    ${CMAKE_CURRENT_SOURCE_DIR}/message_kind.c
    ${CMAKE_CURRENT_SOURCE_DIR}/status.c
    ${CMAKE_CURRENT_SOURCE_DIR}/config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thread_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/task_store_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compute_node_store_adapter.c

    ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
    )

parac_module_def(parac_common "${PARAC_COMMON_SRCS}")
target_link_libraries(parac_common_obj PUBLIC parac_headers parac_modules Threads::Threads)
target_compile_features(parac_common_obj PUBLIC cxx_std_17)
target_link_libraries(parac_common_obj PRIVATE Boost::log fmt::fmt)
set_target_properties(parac_common_obj PROPERTIES C_STANDARD 11)
target_include_directories(parac_common_obj PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(parac_common_obj PUBLIC distrac)

target_include_directories(parac_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

target_link_libraries(parac_common PRIVATE Boost::log fmt::fmt)

target_compile_features(parac_common PUBLIC cxx_std_17)

if(STATIC_BUILDS)
    target_compile_features(parac_common_static PUBLIC cxx_std_17)
    target_include_directories(parac_common_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
endif()

if(ENABLE_TESTS)
    add_library(parac_common_test SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/test_path.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_task_store_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_compute_node_store_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_linked_list.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_registry.cpp
    )
    target_link_libraries(parac_common_test PUBLIC parac_common Catch2::Catch2)

    set(test_libs ${test_libs} parac_common_test CACHE INTERNAL "Libraries for Tests")
endif()
