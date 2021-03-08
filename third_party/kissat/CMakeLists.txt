# Kissat as ExternalProject
include(ExternalProject)

ExternalProject_Add(kissat
    PREFIX kissat
    URL ${CMAKE_CURRENT_SOURCE_DIR}/kissat-sc2020-039805f2.tar.xz
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "C=${CMAKE_C_COMPILER} CFLAGS=-fPIC ./configure && ${MAKE_COMMAND} -C build libkissat.a"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
    )

ExternalProject_Add_Step (
    kissat CopyHeaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/kissat/src/kissat/src/ ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/include/src
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/include/src ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/include/kissat
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/kissat/src/kissat/build/${CMAKE_STATIC_LIBRARY_PREFIX}kissat${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}kissat${CMAKE_STATIC_LIBRARY_SUFFIX}
    DEPENDEES build
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}kissat${CMAKE_STATIC_LIBRARY_SUFFIX}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/build/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/include)

set(libkissat_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/kissat-out/include/")
set(libkissat_INCLUDE_DIRS "${kissat_INCLUDE_DIRS}" PARENT_SCOPE)
set(libkissat_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/kissat-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}kissat${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(libkissat STATIC IMPORTED GLOBAL)
set_property(TARGET libkissat APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libkissat_INCLUDE_DIRS})
set_target_properties(libkissat PROPERTIES IMPORTED_LOCATION ${libkissat_LIBRARIES})

add_dependencies(libkissat kissat)
