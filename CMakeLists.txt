cmake_minimum_required(VERSION 3.10.2)
project(paracooba
    VERSION 0.2.0)

set(BUILD_NUMBER "0" CACHE STRING "Build-Number")
set(BUILD_URL "" CACHE STRING "Unique URL to identify this build.")

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}+${BUILD_NUMBER})

option(STATIC_BUILDS "also build static executable with included modules" ON)
option(DEBUG_ENABLE_ADDRESS_SANITIZER "enable address sanitizer (GCC/Clang)" OFF)
option(DEBUG_ENABLE_THREAD_SANITIZER "enable thread sanitizer (GCC/Clang)" OFF)
option(DEBUG_ENABLE_CLANG_TIDY "enable clang-tidy check of source code" OFF)
option(DEBUG_ENABLE_CLANG_TIDY_WITH_FIX "allow clang-tidy to modify source files and automatically fix issues. Very likely leads to uncompilable code, use selectively." OFF)

option(ENABLE_TRACING_SUPPORT "enable support for writing binary event traces to be able to debug the distributed Paracooba more efficiently." ON)

option(ENABLE_TESTS "enable building and integrating unit tests" ON)

option(ENABLE_DISTRAC "enable distrac support" ON)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/distrac/CMakeLists.txt")
    message(STATUS "Distrac was not downloaded as submodule! Deactivating tracing support.")
    set(ENABLE_DISTRAC OFF)
endif()

if(ENABLE_DISTRAC)
    set(CMAKE_C_FLAGS "-DENABLE_DISTRAC")
    set(CMAKE_CXX_FLAGS "-DENABLE_DISTRAC")
else()
    file(WRITE ${CMAKE_BINARY_DIR}/distrac_paracooba.h "#pragma error 'DISTRAC NOT AVAILABLE!'")
endif()

# Libraries
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Generate Export Header
include(GenerateExportHeader)

# Enable LTO.
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported_result)
if(ipo_supported_result AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message(STATUS "LTO supported. Enable Link Time Optimization on all targets.")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if ( CMAKE_COMPILER_IS_GNUCC )
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wodr")
    endif()
    if ( MSVC )
        set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

set(MAKE_COMMAND "make")
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(MAKE_COMMAND "gmake")
endif()

# Ubuntu standard package is lame.
# set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
add_definitions(-DBOOST_ALL_DYN_LINK)

set(Boost_USE_MULTITHREADED ON)
find_package(Boost COMPONENTS log program_options thread filesystem system iostreams coroutine context date_time REQUIRED)

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
    if(POLICY CMP0079)
        cmake_policy(SET CMP0079 NEW)
    endif()
    include(CTest)
    include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/catch/Catch2-2.13.8/contrib/Catch.cmake)
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

# Thread Sanitizer for Debug Builds
if(DEBUG_ENABLE_THREAD_SANITIZER)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=thread")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread")
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
    target_include_directories(${NAME}_obj PRIVATE ${Boost_INCLUDE_DIRS})

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
    parac_static_shared_combined(${NAME} "${SRCS}" ${DISCOVER_SRC_FILE})

    set_target_properties(${NAME}_obj PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME}_obj PROPERTIES C_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)
    set_target_properties(${NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

    # Add fmt support to parac_log statements and set FMT to be header only, as
    # the runtime will be defined by the executable and not in the module.
    target_compile_definitions(${NAME}_obj PRIVATE "-DPARAC_LOG_INCLUDE_FMT -DFMT_HEADER_ONLY")

    get_target_property(FMT_INCLUDE_DIRECTORIES fmt::fmt INCLUDE_DIRECTORIES)
    target_include_directories(${NAME}_obj PRIVATE ${FMT_INCLUDE_DIRECTORIES})

    get_target_property(PARAC_COMMON_INCLUDE_DIRS parac_common INCLUDE_DIRECTORIES)
    target_include_directories(${NAME}_obj PUBLIC ${PARAC_COMMON_INCLUDE_DIRS})

    get_target_property(PARAC_MODULES_INCLUDE_DIRS parac_modules INCLUDE_DIRECTORIES)
    target_include_directories(${NAME}_obj PUBLIC ${PARAC_MODULES_INCLUDE_DIRS})

    if(ENABLE_DISTRAC)
        get_target_property(DISTRAC_INCLUDE_DIRS distrac INCLUDE_DIRECTORIES)
        target_include_directories(${NAME}_obj PUBLIC ${DISTRAC_INCLUDE_DIRS})
        target_link_libraries(${NAME} PUBLIC distrac)
    endif()

    set_target_properties(${NAME}_obj PROPERTIES C_STANDARD 11)
    target_compile_features(${NAME}_obj PUBLIC cxx_std_17)
    set_property(TARGET ${NAME}_obj PROPERTY CXX_STANDARD 17)

    set_property(TARGET ${NAME}_obj APPEND PROPERTY INTERFACE_LINK_LIBRARIES distrac parac_common parac_modules)
    target_link_libraries(${NAME} PUBLIC parac_modules fmt::fmt)

    target_include_directories(${NAME}_obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(${NAME}_obj PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    if(STATIC_BUILDS)
        set_target_properties(${NAME}_static PROPERTIES CXX_VISIBILITY_PRESET hidden)
        set_target_properties(${NAME}_static PROPERTIES C_VISIBILITY_PRESET hidden)
        target_compile_definitions(${NAME}_static PUBLIC "-DPARAC_USE_STATIC_${NAME}")
    endif()
endmacro()

set(test_libs  "" CACHE INTERNAL "Libraries for Tests")

if(ENABLE_DISTRAC)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/distrac_paracooba.h
        COMMAND $<TARGET_FILE:distrac-codegen-bin> ${CMAKE_CURRENT_SOURCE_DIR}/Paracooba.distracdef -o ${CMAKE_CURRENT_BINARY_DIR}/distrac_paracooba.h
        DEPENDS distrac-codegen-bin
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Paracooba.distracdef)
    add_custom_target(distrac_paracooba DEPENDS distrac_paracooba.h)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(loader)
add_subdirectory(modules)
add_subdirectory(executable)

if(ENABLE_DISTRAC)
    add_subdirectory(distracvis wd_distracvis)
endif()

set_source_files_properties(${CMAKE_BINARY_DIR}/distrac_paracooba.h PROPERTIES GENERATED 1)

add_executable(parac $<TARGET_OBJECTS:parac_executable>
    ${CMAKE_CURRENT_SOURCE_DIR}/loader/StaticModuleLoader.cpp)
target_link_libraries(parac PUBLIC distrac Threads::Threads parac_common parac_modules parac_loader Boost::filesystem Boost::coroutine Boost::context Boost::program_options Boost::date_time ${CMAKE_DL_LIBS} fmt)

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(parac PUBLIC "-lstdthreads")
endif()

if(STATIC_BUILDS)
    add_executable(paracs $<TARGET_OBJECTS:parac_executable>
        ${CMAKE_CURRENT_SOURCE_DIR}/loader/StaticModuleLoader.cpp)
    target_link_libraries(paracs parac_communicator_static parac_broker_static parac_runner_static parac_solver_static distrac parac_common_static)
    target_link_libraries(paracs distrac parac_modules parac_loader fmt::fmt Boost::filesystem Boost::coroutine Boost::log Boost::context Boost::program_options Boost::date_time ${CMAKE_DL_LIBS})
endif()

if(STATIC_BUILDS)
    add_executable(paraqs $<TARGET_OBJECTS:parac_executable>
        ${CMAKE_CURRENT_SOURCE_DIR}/loader/StaticModuleLoader.cpp)
    target_link_libraries(paraqs parac_communicator_static parac_broker_static parac_runner_static parac_solver_qbf_static distrac parac_common_static)
    target_link_libraries(paraqs distrac parac_modules parac_loader fmt::fmt Boost::filesystem Boost::coroutine Boost::log Boost::context Boost::program_options Boost::date_time ${CMAKE_DL_LIBS})
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
