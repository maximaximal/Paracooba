# Files
add_subdirectory(src)
add_subdirectory(capnp-schemas)

# Library
add_library(paracuber SHARED ${PARACUBER_SRCS} $<TARGET_OBJECTS:capnp_schemas_lib>)
set_property(TARGET paracuber PROPERTY CXX_STANDARD 17)
target_include_directories(paracuber PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(paracuber PUBLIC ${libcadical_INCLUDE_DIRS})

target_include_directories(paracuber PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if(BOOST_BEAST_AVAILABLE)
    target_include_directories(paracuber PRIVATE ${BOOST_BEAST_INCLUDE_DIRS})
    target_compile_definitions(paracuber PRIVATE ENABLE_INTERNAL_WEBSERVER)
endif()

target_link_libraries(paracuber PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(paracuber PUBLIC Boost::log Boost::program_options Boost::thread Boost::filesystem)
target_link_libraries(paracuber PRIVATE cereal)
target_link_libraries(paracuber PRIVATE libcadical)

include(GNUInstallDirs)

install(TARGETS paracuber
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/paracuber)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/paracuber
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/

    PATTERN "*~" EXCLUDE)
