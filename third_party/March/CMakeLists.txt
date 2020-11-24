# March as ExternalProject
include(ExternalProject)

ExternalProject_Add(March
    PREFIX March
    URL ${CMAKE_CURRENT_SOURCE_DIR}/March_cu.tar.gz
    URL_HASH SHA256=244085144b66c8f54f868cfb2f8f8078cda56bdf00c0b4b97faf34af5b2143a6
    BUILD_IN_SOURCE 1
    BUILD_COMMAND bash -c "make"
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    CONFIGURE_COMMAND ""
)
ExternalProject_add_Step (
  March copyBinary
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/March/src/March/march_cu ${CMAKE_CURRENT_BINARY_DIR}/
  DEPENDEES build
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/march-out)
