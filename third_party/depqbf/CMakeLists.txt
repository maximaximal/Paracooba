ExternalProject_Add(depqbf
    PREFIX depqbf
    URL ${CMAKE_CURRENT_SOURCE_DIR}/depqbf-version-6.03.tar.xz
    URL_HASH SHA256=f7c2a7de6340317d82caa478702bbc8c82b93e1a093f0e4eb26c7ddd8c91372b
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=\"-fPIC -fvisibility=hidden\" CXXFLAGS=\"-fPIC -fvisibility=hidden\" ./compile.sh"
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
