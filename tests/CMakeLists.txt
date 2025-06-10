project(Tests LANGUAGES CXX)

enable_testing()

#find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Test)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(ParserTest tst_parsertest.cpp helpers.h)
add_test(NAME ParserTest COMMAND ParserTest)
target_link_libraries(ParserTest PRIVATE Qt${QT_VERSION_MAJOR}::Test PollocadCore)

add_executable(ExecutorTest tst_executortest.cpp helpers.h)
add_test(NAME ExecutorTest COMMAND ParserTest)
target_link_libraries(ExecutorTest PRIVATE Qt${QT_VERSION_MAJOR}::Test PollocadCore)
