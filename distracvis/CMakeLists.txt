set(DISTRACVIS_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/distracvis_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/treevis_widget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mainwindow.cpp
    )

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/modules")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM gtkmm-3.0)
find_package(Magnum COMPONENTS GL QUIET)

if(Magnum_FOUND AND GTKMM_FOUND)

    if(MAGNUM_TARGET_GLES AND NOT MAGNUM_TARGET_DESKTOP_GLES)
        find_package(Magnum REQUIRED EglContext)
    elseif(CORRADE_TARGET_WINDOWS)
        find_package(Magnum REQUIRED WglContext)
    elseif(CORRADE_TARGET_APPLE)
        find_package(Magnum REQUIRED CglContext)
    elseif(CORRADE_TARGET_UNIX)
        find_package(Magnum REQUIRED GlxContext)
    else()
        message(FATAL_ERROR "Magnum context creation is not supported on this platform")
    endif()

    add_executable(distracvis ${DISTRACVIS_SRCS})

    target_link_libraries(distracvis PRIVATE
        Magnum::GLContext
        Magnum::GL
        Magnum::Magnum
        ${GTKMM_LIBRARIES}
        distrac-analysis)

    target_compile_features(distracvis PRIVATE cxx_std_17)

    target_include_directories(distracvis SYSTEM PRIVATE ${GTKMM_INCLUDE_DIRS})

    set_property(TARGET distracvis PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
else()
    message(STATUS "Cannot build distracvisu, as either Magnum or gtkmm or both were not found.")
endif()
