FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-3ed9397.tar.gz
  URL_HASH  SHA256=826c05e9679aa8a2cafe717985d22b1e6c157e30cceafccbd527b0d04e995eba
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
