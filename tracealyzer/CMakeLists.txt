add_subdirectory(src)

add_executable(parac_tracealyzer ${TRACEALYZER_SRCS})

set_property(TARGET parac_tracealyzer PROPERTY CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTKMM gtkmm-3.0)

target_include_directories(parac_tracealyzer PRIVATE ${GTKMM_INCLUDE_DIRS})

target_link_libraries(parac_tracealyzer PUBLIC Boost::program_options Boost::regex Boost::filesystem Boost::iostreams ${GTKMM_LIBRARIES})

if(GTKMM_FOUND)
    target_compile_definitions(parac_tracealyzer PUBLIC ENABLE_INTERACTIVE_VIEWER)
endif()
