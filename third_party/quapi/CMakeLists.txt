FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/QuAPI-paar2022-enhanced.tar.gz
  URL_HASH  SHA256=8099661eebb7e21e42186f01b78ad03bbe93c3e802ffa9f9c1712c1fb8a968f0
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
