# CaDiCal as ExternalProject
include(ExternalProject)
include(FetchContent)

add_subdirectory(cadical)
add_subdirectory(kissat)

add_subdirectory(depqbf)

add_subdirectory(catch)

set(libcadical_INCLUDE_DIRS ${libcadical_INCLUDE_DIRS} PARENT_SCOPE)
set(libcadical_LIBRARIES ${libcadical_LIBRARIES} PARENT_SCOPE)

add_subdirectory(cereal)
add_subdirectory(fmt)

if(ENABLE_DISTRAC)
    add_subdirectory(distrac)
else()
    add_library(distrac INTERFACE)
endif()

add_subdirectory(quapi)
