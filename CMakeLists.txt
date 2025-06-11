list(PREPEND CMAKE_MODULE_PATH "/home/matti/devel/OCCT")

cmake_minimum_required(VERSION 3.16)

project(pollocad VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Quick Test)

qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(core)
add_subdirectory(tests)

qt_add_executable(PollocadGui
    main.cpp
)

qt_add_qml_module(PollocadGui
    URI pollocad
    VERSION 1.0
    QML_FILES Main.qml CodeEditor.qml
    SOURCES occtview.h occtview.cpp
    RESOURCES notes.txt
)

set_target_properties(PollocadGui PROPERTIES
    OUTPUT_NAME pollocad_gui
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(PollocadGui
    PRIVATE
        PollocadCore
        Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS PollocadGui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
