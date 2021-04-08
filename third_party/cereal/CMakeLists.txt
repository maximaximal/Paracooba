add_library(cereal INTERFACE)
target_include_directories(cereal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cereal-1.3.0/include)

target_compile_definitions(cereal INTERFACE CEREAL_THREAD_SAFE)
