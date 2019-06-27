cmake_minimum_required(VERSION 3.8)
project(paracuber
    VERSION 0.1.0)

# Libraries
find_package(Threads)

# Ubuntu standard package is lame.
# set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_ALL_DYN_LINK)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS log program_options thread filesystem REQUIRED)

find_package(CapnProto REQUIRED)

add_subdirectory(third_party)

add_subdirectory(libparacuber)

# Files
add_subdirectory(src)

# Basenames for logging
# This macro has been taken from https://stackoverflow.com/a/27990434
function(define_file_basename_for_sources targetname)
    get_target_property(source_files "${targetname}" SOURCES)
    foreach(sourcefile ${source_files})
	# Get source file's current list of compile definitions.
	get_property(defs SOURCE "${sourcefile}"
	    PROPERTY COMPILE_DEFINITIONS)
	# Add the FILE_BASENAME=filename compile definition to the list.
	get_filename_component(basename "${sourcefile}" NAME)
	list(APPEND defs "FILE_BASENAME=\"${basename}\"")
	# Set the updated compile definitions on the source file.
	set_property(
	    SOURCE "${sourcefile}"
	    PROPERTY COMPILE_DEFINITIONS ${defs})
    endforeach()
endfunction()

# Executable
add_executable(parac ${SRCS})
set_property(TARGET parac PROPERTY CXX_STANDARD 17)

define_file_basename_for_sources(paracuber)
define_file_basename_for_sources(parac)

target_link_libraries(parac PUBLIC paracuber)
target_link_libraries(parac PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(parac PUBLIC Boost::log Boost::program_options)
target_include_directories(parac PUBLIC ${libcadical_INCLUDE_DIRS})

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc ALL
	${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
	)
endif(DOXYGEN_FOUND)
