ExternalProject_Add(depqbf
    PREFIX depqbf
    URL ${CMAKE_CURRENT_SOURCE_DIR}/depqbf-version-6.03.tar.xz
    URL_HASH SHA256=c35c071534ac4a6d9b7e464412de797db5eb2c10923fd8c8434f63a9edd722f3
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "CXX=${CMAKE_CXX_COMPILER} CFLAGS=\"-fPIC -fvisibility=hidden\" CXXFLAGS=\"-fPIC -fvisibility=hidden\" ./compile.sh"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/depqbf/src/depqbf/${CMAKE_STATIC_LIBRARY_PREFIX}qdpll${CMAKE_STATIC_LIBRARY_SUFFIX}
)

set(libdepqbf_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/depqbf/src/depqbf/")
set(libdepqbf_INCLUDE_DIRS "${libdepqbf_INCLUDE_DIRS}" PARENT_SCOPE)
set(libdepqbf_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/depqbf/src/depqbf/${CMAKE_STATIC_LIBRARY_PREFIX}qdpll${CMAKE_STATIC_LIBRARY_SUFFIX})

add_library(libdepqbf STATIC IMPORTED GLOBAL)
set_property(TARGET libdepqbf APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${libdepqbf_INCLUDE_DIRS})
set_target_properties(libdepqbf PROPERTIES IMPORTED_LOCATION ${libdepqbf_LIBRARIES})

add_dependencies(libdepqbf depqbf)
