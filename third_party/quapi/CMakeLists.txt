FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-96beded7.tar.gz
  URL_HASH  SHA256=ac04e90fcc7c6a8be95fffef463c7eb3f1e60c6f1025b8bf493dd19a6b14ff11
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
