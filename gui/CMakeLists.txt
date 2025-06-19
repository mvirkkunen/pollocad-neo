list(PREPEND CMAKE_MODULE_PATH "/home/matti/devel/OCCT")

qt_add_executable(PollocadGui
    src/main.cpp
)

qt_add_qml_module(PollocadGui
    URI pollocadgui
    VERSION 1.0
    SOURCES src/occtview.h src/occtview.cpp src/codetextarea.h src/codetextarea.cpp src/backgroundexecutor.h src/backgroundexecutor.cpp
    QML_FILES qml/Main.qml qml/CodeEditor.qml
    RESOURCES qml/icon.png
)

set_target_properties(PollocadGui PROPERTIES
    OUTPUT_NAME pollocad
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(PollocadGui PRIVATE src)

target_link_libraries(PollocadGui
    PRIVATE
        PollocadCore
        Qt6::Quick
        Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS PollocadGui
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
