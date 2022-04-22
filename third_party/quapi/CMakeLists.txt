FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-376d9267.tar.bz2
  URL_HASH  SHA256=38a685b2e249fe047894086f7dcff66119fd2cff169d1eb73627678097ff7c53
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
