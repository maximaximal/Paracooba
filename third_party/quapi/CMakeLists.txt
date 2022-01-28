FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-4ce76e29.tar.bz2
  URL_HASH  SHA256=0fbfff8fe36e6b32070fb4dbc40de6e81cf15226cabcf3804b8224c0c26a5581
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
