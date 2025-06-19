cmake_minimum_required(VERSION 3.16)

project(PollocadProject VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Test)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

add_subdirectory(gui)
add_subdirectory(core)
add_subdirectory(tests)