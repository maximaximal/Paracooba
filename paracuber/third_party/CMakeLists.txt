# CaDiCal as ExternalProject
include(ExternalProject)

ExternalProject_Add(cadical
    PREFIX cadical
    URL ${CMAKE_CURRENT_SOURCE_DIR}/cadical-1.0.0-c861e12.tar.xz
    URL_HASH SHA256=77ae686bb2db5ffcc94671ad851940b0863f6373e6a89e26ac43c9bf7e678e32
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "CXXFLAGS=-fPIC ./configure -q && make cadical"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
)
ExternalProject_Add_Step (
    cadical CopyHeaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/cadical/src/cadical/src/ ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/src
    COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/src ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/cadical
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/cadical/src/cadical/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX}
    DEPENDEES build
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/)
# This is not recognized by older CMake versions.
#file(TOUCH ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX})
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include)

set(libcadical_INCLUDE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/ PARENT_SCOPE)
set(libcadical_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(libcadical STATIC IMPORTED GLOBAL)
#target_include_directories(libcadical INTERFACE ${libcadical_INCLUDE_DIRS})
set_target_properties(libcadical PROPERTIES IMPORTED_LOCATION ${libcadical_LIBRARIES})

add_dependencies(libcadical cadical)
