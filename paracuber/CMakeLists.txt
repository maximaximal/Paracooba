cmake_minimum_required(VERSION 3.8)
project(paracuber
    VERSION 0.1.0)

# Libraries
find_package(Threads)

set(Boost_USE_STATIC_LIBS ON)  
set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS log program_options)

add_subdirectory(libparacuber)

# Files
add_subdirectory(src)

# Executable
add_executable(parac ${SRCS})
set_property(TARGET parac PROPERTY CXX_STANDARD 17)

target_link_libraries(parac PUBLIC paracuber)
target_link_libraries(parac PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(parac PUBLIC Boost::log Boost::program_options)
