set(LOGALYZER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/logalyzer.cpp
    )

add_executable(parac_logalyzer ${LOGALYZER_SRCS})

set_property(TARGET parac_logalyzer PROPERTY CXX_STANDARD 17)

target_link_libraries(parac_logalyzer PUBLIC Boost::program_options Boost::regex Boost::filesystem)
