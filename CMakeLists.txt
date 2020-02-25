cmake_minimum_required(VERSION 3.8)
project(paracuber
    VERSION 0.1.0)


set(BUILD_NUMBER "0" CACHE STRING "Build-Number")
set(BUILD_URL "" CACHE STRING "Unique URL to identify this build.")

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}+${BUILD_NUMBER})

option(DEBUG_ENABLE_ADDRESS_SANITIZER "enable address sanitizer (GCC)" OFF)

# Libraries
find_package(Threads)

# Ubuntu standard package is lame.
# set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_ALL_DYN_LINK)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS log program_options thread filesystem system REQUIRED)

# Try to include beast headers. They are required for the internal webserver.
find_file(BOOST_BEAST_HEADER "boost/beast/core.hpp" PATHS ${Boost_INCLUDE_DIRS})
if(NOT BOOST_BEAST_HEADER)
    message(WARNING "Could not find Boost Beast HTTP Library. Internal webserver will not be available.")
    set(BOOST_BEAST_AVAILABLE FALSE)
else()
    message(STATUS "Found Boost Beast HTTP library. Enabling internal webserver.")
    set(BOOST_BEAST_AVAILABLE TRUE)
endif()

# Add Catch2 unit testing framework.
include(CTest)
include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch/Catch2-2.9.2/contrib/Catch.cmake)
enable_testing()

add_subdirectory(third_party)

# Address Sanitizer for Debug Builds
if(DEBUG_ENABLE_ADDRESS_SANITIZER)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
endif()

add_subdirectory(libparacuber)

# Files
add_subdirectory(src)
add_subdirectory(test)

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

# Installation logic.
include(GNUInstallDirs)

install(TARGETS parac
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static)

# Documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
	${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Generating API documentation with Doxygen" VERBATIM
	)
endif(DOXYGEN_FOUND)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/parac.sh
    DESTINATION /usr/share/bash-completion/completions/parac)

# Packaging
set(CPACK_GENERATOR "DEB")

set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Distributed SAT Solver based on CaDiCaL")
set(CPACK_PACKAGE_NAME "ParaCuber")
set(CPACK_PACKAGE_VENDOR "FMV")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESSOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md)
set(CPACK_RESSOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/packages)
set(CPACK_PACKAGE_CONTACT "Max Heisinger <maximilian.heisinger@jku.at>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libboost-system1.65.1,
    libboost-log1.65.1,
    libboost-program-options1.65.1")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

include(CPack)
