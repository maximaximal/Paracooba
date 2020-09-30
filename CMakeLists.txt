cmake_minimum_required(VERSION 3.9)
project(paracooba
    VERSION 0.1.0)

set(BUILD_NUMBER "0" CACHE STRING "Build-Number")
set(BUILD_URL "" CACHE STRING "Unique URL to identify this build.")

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}+${BUILD_NUMBER})

option(STATIC_BUILDS "also build static executable with included modules" ON)
option(DEBUG_ENABLE_ADDRESS_SANITIZER "enable address sanitizer (GCC)" OFF)
option(DEBUG_ENABLE_CLANG_TIDY "enable clang-tidy check of source code" OFF)
option(DEBUG_ENABLE_CLANG_TIDY_WITH_FIX "allow clang-tidy to modify source files and automatically fix issues. Very likely leads to uncompilable code, use selectively." OFF)

option(ENABLE_TRACING_SUPPORT "enable support for writing binary event traces to be able to debug the distributed Paracooba more efficiently." ON)

option(ENABLE_TESTS "enable building and integrating unit tests" ON)

# Libraries
find_package(Threads)

# Generate Export Header
include(GenerateExportHeader)

# Enable LTO.
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported_result)
if(ipo_supported_result AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message(STATUS "LTO supported. Enable Link Time Optimization on all targets.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if ( CMAKE_COMPILER_IS_GNUCC )
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    endif()
    if ( MSVC )
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

# Ubuntu standard package is lame.
# set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_ALL_DYN_LINK)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS log program_options thread filesystem system iostreams coroutine context REQUIRED)

# Try to include beast headers. They are required for the internal webserver.
find_file(BOOST_BEAST_HEADER "boost/beast/core.hpp" PATHS ${Boost_INCLUDE_DIRS})
if(NOT BOOST_BEAST_HEADER)
    message(WARNING "Could not find Boost Beast HTTP Library. Internal webserver will not be available.")
    set(BOOST_BEAST_AVAILABLE FALSE)
else()
    message(STATUS "Found Boost Beast HTTP library. Enabling internal webserver.")
    set(BOOST_BEAST_AVAILABLE TRUE)
endif()

find_program(
    CLANG_TIDY_EXE
    NAMES "clang-tidy"
    DOC "Path to clang-tidy executable"
)
if(NOT CLANG_TIDY_EXE)
    message(STATUS "clang-tidy not found.")
else()
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE} - Enable checking with -DDEBUG_ENABLE_CLANG_TIDY=ON in CMake. Warning: Compilation Overhead!")

    set(CLANG_TIDY_ARG_FIX "")
    if(DEBUG_ENABLE_CLANG_TIDY_WITH_FIX)
        set(CLANG_TIDY_ARG_FIX "-fix")
    endif()

    set(DO_CLANG_TIDY "${CLANG_TIDY_EXE}" ${CLANG_TIDY_ARG_FIX} "-checks=*,-clang-analyzer-alpha.*,-llvm-namespace-comment,-google-readability-namespace-comments,-fuchsia*,-modernize-use-trailing-return-type,-hicpp-no-array-decay,-cppcoreguidelines-*,-modernize-use-equals-default,-*braces-around-statements,-hicpp-avoid-goto,-modernize-concat-nested-namespaces,-readability-implicit-bool-conversion")
endif()

find_program(CCACHE_PROGRAM ccache)
# Speed up compilations using ccache.
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache at ${CCACHE_PROGRAM} - using it to speed up compilations.")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Add Catch2 unit testing framework.
if(ENABLE_TESTS)
    cmake_policy(SET CMP0079 NEW)
    include(CTest)
    include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch/Catch2-2.13.0/contrib/Catch.cmake)
    enable_testing()
endif()

# Address Sanitizer for Debug Builds
if(DEBUG_ENABLE_ADDRESS_SANITIZER)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DBOOST_USE_ASAN")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address  -fsanitize=undefined -fsanitize=address -fno-sanitize-recover=undefined -fno-sanitize=vptr")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address  -fsanitize=undefined -fsanitize=address -fno-sanitize-recover=undefined -fno-sanitize=vptr")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address  -fsanitize=undefined -fsanitize=address -fno-sanitize-recover=undefined -fno-sanitize=vptr")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address  -fsanitize=undefined -fsanitize=address -fno-sanitize-recover=undefined -fno-sanitize=vptr")
endif()

add_subdirectory(third_party)

add_library(parac_headers INTERFACE)
target_include_directories(parac_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

macro(parac_static_shared_combined NAME SRCS SHARED_EXTRA_SRCS)
    add_library(${NAME}_obj OBJECT ${SRCS})
    set_property(TARGET ${NAME}_obj PROPERTY POSITION_INDEPENDENT_CODE ON)
    generate_export_header(${NAME}_obj
        BASE_NAME ${NAME}
    )
    target_include_directories(${NAME}_obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    add_library(${NAME} SHARED $<TARGET_OBJECTS:${NAME}_obj> ${SHARED_EXTRA_SRCS})
    if(STATIC_BUILDS)
        add_library(${NAME}_static STATIC $<TARGET_OBJECTS:${NAME}_obj>)
    endif()

    target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${NAME} PRIVATE parac_modules)
endmacro()

macro(parac_module_def NAME SRCS)
    parac_static_shared_combined(${NAME} "${SRCS}" "")
endmacro()

macro(parac_module NAME SRCS DISCOVER_SRC_FILE)
    parac_static_shared_combined(${NAME} ${SRCS} ${DISCOVER_SRC_FILE})

    set_target_properties(${NAME}_obj PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME}_obj PROPERTIES C_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

    target_compile_definitions(${NAME}_obj PRIVATE "-DPARAC_LOG_INCLUDE_FMT")

    target_link_libraries(${NAME}_obj PUBLIC parac_common parac_modules fmt::fmt)
    target_include_directories(${NAME}_obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    if(STATIC_BUILDS)
        set_target_properties(${NAME}_static PROPERTIES CXX_VISIBILITY_PRESET hidden)
        set_target_properties(${NAME}_static PROPERTIES C_VISIBILITY_PRESET hidden)
        target_compile_definitions(${NAME}_static PUBLIC "-DPARAC_USE_STATIC_${NAME}")
    endif()
endmacro()

set(test_libs  "" CACHE INTERNAL "Libraries for Tests")

add_subdirectory(modules)
add_subdirectory(executable)

add_executable(parac $<TARGET_OBJECTS:parac_executable>
    ${CMAKE_CURRENT_SOURCE_DIR}/executable/StaticModuleLoader.cpp)
target_link_libraries(parac PUBLIC parac_common parac_modules Boost::filesystem Boost::coroutine Boost::context Boost::program_options ${CMAKE_DL_LIBS} fmt)

if(STATIC_BUILDS)
    add_executable(paracs $<TARGET_OBJECTS:parac_executable>
        ${CMAKE_CURRENT_SOURCE_DIR}/executable/StaticModuleLoader.cpp)
    target_link_libraries(paracs parac_common_static parac_modules fmt::fmt Boost::filesystem Boost::coroutine Boost::log Boost::context Boost::program_options ${CMAKE_DL_LIBS})
    target_link_libraries(paracs parac_communicator_static parac_broker_static parac_runner_static parac_solver_static)
endif()

if(ENABLE_TESTS)
    add_subdirectory(test)
endif()

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
set(CPACK_PACKAGE_NAME "Paracooba")
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
