# Files
add_subdirectory(src)

# Library
add_library(paracuber SHARED ${PARACUBER_SRCS})
set_property(TARGET paracuber PROPERTY CXX_STANDARD 17)
target_include_directories(paracuber PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(paracuber PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(paracuber PUBLIC Boost::log Boost::program_options)
target_link_libraries(paracuber PRIVATE libcadical)
