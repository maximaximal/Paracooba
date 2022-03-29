FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-dffee29c.tar.bz2
  URL_HASH  SHA256=7ecd2090e510c579e148d50df46d1490397f5d6f4d9ef84b5ba1ddfca26c74dd
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
