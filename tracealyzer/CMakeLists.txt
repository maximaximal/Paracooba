add_subdirectory(src)

add_executable(parac_tracealyzer ${TRACEALYZER_SRCS})

set_property(TARGET parac_tracealyzer PROPERTY CXX_STANDARD 17)

target_link_libraries(parac_tracealyzer PUBLIC Boost::program_options Boost::regex Boost::filesystem Boost::iostreams)
