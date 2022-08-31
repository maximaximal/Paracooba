FetchContent_Declare(quapi
  URL       ${CMAKE_CURRENT_SOURCE_DIR}/quapi-39c44b6.tar.gz
  URL_HASH  SHA256=aba284150cb1f1359d9acbc6804fa44e1fef5028961756d8cffdd116fb6f56da
)

FetchContent_GetProperties(quapi)
if(NOT quapi_POPULATED)
  FetchContent_Populate(quapi)
  add_subdirectory(${quapi_SOURCE_DIR} ${quapi_BINARY_DIR})
endif()
