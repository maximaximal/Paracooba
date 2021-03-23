# CaDiCal as ExternalProject
include(ExternalProject)

set(CADICAL_SANITIZER_OPTIONS "")

if(DEBUG_ENABLE_ADDRESS_SANITIZER)
    set(CADICAL_SANITIZER_OPTIONS "-fsanitize=address")
endif()

ExternalProject_Add(cadical
    PREFIX cadical
    URL ${CMAKE_CURRENT_SOURCE_DIR}/cadical-34661ad.tar.gz
    URL_HASH SHA256=c079b3951d2a98c60ae61311f2fc5225519986edc1724c90b257f36839aa8b15
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "./configure -q CXX=${CMAKE_CXX_COMPILER} CXXFLAGS=\"-fPIC ${CADICAL_SANITIZER_OPTIONS} -fvisibility=hidden\" && ${MAKE_COMMAND} -C build libcadical.a"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
)
ExternalProject_Add_Step (
    cadical CopyHeaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/cadical/src/cadical/src/ ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/src
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/src ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/cadical
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/cadical/src/cadical/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX} ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX}
    DEPENDEES build
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include)

set(libcadical_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/cadical-out/include/")
set(libcadical_INCLUDE_DIRS "${libcadical_INCLUDE_DIRS}" PARENT_SCOPE)
set(libcadical_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/cadical-out/build/${CMAKE_STATIC_LIBRARY_PREFIX}cadical${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(libcadical STATIC IMPORTED GLOBAL)
set_property(TARGET libcadical APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libcadical_INCLUDE_DIRS})
set_target_properties(libcadical PROPERTIES IMPORTED_LOCATION ${libcadical_LIBRARIES})

add_dependencies(libcadical cadical)
