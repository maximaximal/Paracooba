set(PARAC_LOADER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ModuleLoader.cpp
    )

add_library(parac_loader STATIC ${PARAC_LOADER_SRCS})

target_compile_features(parac_loader PUBLIC cxx_std_17)

target_include_directories(parac_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(parac_loader PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(parac_loader PUBLIC parac_modules fmt ${CMAKE_DL_LIBS})

target_compile_definitions(parac_loader PRIVATE "-DPARAC_LOG_INCLUDE_FMT")

set_property(TARGET parac_loader PROPERTY POSITION_INDEPENDENT_CODE ON)

generate_export_header(parac_loader)
