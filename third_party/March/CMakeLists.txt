# March as ExternalProject
cmake_minimum_required(VERSION 3.17)

include(ExternalProject)

ExternalProject_Add(March
    PREFIX March
    URL ${CMAKE_CURRENT_SOURCE_DIR}/March_cu.tar.gz
    URL_HASH SHA256=0711199168585c89d3940d4a1ac3d31a93619d24b34b39d4272377ef7b080491
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
