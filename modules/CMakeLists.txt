set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

add_subdirectory(commonc)

add_library(parac_modules INTERFACE)

target_link_libraries(parac_modules INTERFACE parac_headers)
