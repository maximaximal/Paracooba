set(PARAC_EXECUTABLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ModuleLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CLI.cpp)

add_library(parac_executable OBJECT ${PARAC_EXECUTABLE_SRCS})
set_property(TARGET parac_executable PROPERTY CMAKE_C_VISIBILITY_PRESET default)
set_property(TARGET parac_executable PROPERTY CMAKE_CXX_VISIBILITY_PRESET default)

set_property(TARGET parac_executable PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(parac_executable PROPERTIES CXX_STANDARD 17)
target_compile_definitions(parac_executable PRIVATE "-DPARAC_LOG_INCLUDE_FMT")
target_link_libraries(parac_executable PUBLIC parac_common parac_modules Boost::filesystem Boost::coroutine Boost::context Boost::program_options ${CMAKE_DL_LIBS} fmt)
target_include_directories(parac_executable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
