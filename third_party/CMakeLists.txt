add_subdirectory(cadical)
add_subdirectory(catch)

set(libcadical_INCLUDE_DIRS ${libcadical_INCLUDE_DIRS} PARENT_SCOPE)
set(libcadical_LIBRARIES ${libcadical_LIBRARIES} PARENT_SCOPE)

add_subdirectory(cereal)
add_subdirectory(March)
