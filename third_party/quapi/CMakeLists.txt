FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-c1a648d.tar.gz
  URL_HASH  SHA256=c5c7d599b57cdf7071e1a34dd2be1343a31f72c20892c23784aacbc97d4de1e6
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
