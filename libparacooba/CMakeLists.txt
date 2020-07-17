# Files
add_subdirectory(src)
add_subdirectory(capnp-schemas)

# Library
add_library(paracooba STATIC ${PARACOOBA_SRCS})
set_property(TARGET paracooba PROPERTY CXX_STANDARD 17)
target_include_directories(paracooba PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(paracooba PUBLIC ${libcadical_INCLUDE_DIRS})

target_include_directories(paracooba PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(BOOST_BEAST_AVAILABLE)
    target_include_directories(paracooba PRIVATE ${BOOST_BEAST_INCLUDE_DIRS})
    target_compile_definitions(paracooba PUBLIC ENABLE_INTERNAL_WEBSERVER)
endif()

if(ENABLE_TRACING_SUPPORT)
    target_compile_definitions(paracooba PUBLIC PARACOOBA_ENABLE_TRACING_SUPPORT)
endif()

target_link_libraries(paracooba PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(paracooba PUBLIC Boost::log Boost::program_options Boost::thread Boost::filesystem)
target_link_libraries(paracooba PRIVATE cereal)
target_link_libraries(paracooba PRIVATE libcadical)

include(GNUInstallDirs)

install(TARGETS paracooba
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/paracooba)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/paracooba
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/

    PATTERN "*~" EXCLUDE)
