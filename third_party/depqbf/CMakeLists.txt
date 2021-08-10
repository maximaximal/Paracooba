ExternalProject_Add(depqbf
    PREFIX depqbf
    URL ${CMAKE_CURRENT_SOURCE_DIR}/depqbf-version-6.03.tar.xz
    URL_HASH SHA256=8e8b0ac186392f924d70298e610e8d03707189a57dd3dcb9a89194061768ae1f
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
