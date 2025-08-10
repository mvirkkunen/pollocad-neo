set(POLLO_STL "${CMAKE_CURRENT_SOURCE_DIR}/Chicken_by_MartinSivecky_892445.stl")
set(POLLO_HEX "${CMAKE_CURRENT_BINARY_DIR}/generated_inc/generated/pollo.hex")

add_library(PollocadCore
    parser.h parser.cpp
    ast.h ast.cpp
    value.h value.cpp
    contexts.cpp contexts.h
    executor.h executor.cpp
    logmessage.h
    builtins/helpers.h builtins/helpers.cpp
    builtins/chamfer_fillet.cpp
    builtins/make_2d.cpp
    builtins/make_3d.cpp
    builtins/primitives.cpp
    builtins/shape_manipulation.cpp
)

target_include_directories(PollocadCore
    PUBLIC
        ../lexy/include
        /home/matti/devel/OCCT/build/include/opencascade
        ${CMAKE_CURRENT_BINARY_DIR}/generated_inc
        .)

target_link_directories(PollocadCore PUBLIC /home/matti/devel/OCCT/build/lin64/gcc/libi)

target_link_libraries(PollocadCore
    PUBLIC
        TKBO
        TKBRep
        TKDESTEP
        TKernel
        TKFillet
        TKMath
        TKOffset
        TKPrim
        TKService
        TKTopAlgo
)

find_program(XXD_COMMAND NAMES xxd)

if(NOT XXD_COMMAND)
    message(FATAL_ERROR "xxd not found")
endif()

add_custom_command(
    OUTPUT "${POLLO_HEX}"
    COMMAND "${XXD_COMMAND}" -i < "${POLLO_STL}" > "${POLLO_HEX}"
    MAIN_DEPENDENCY "${POLLO_STL}"
)
add_custom_target(PolloStl DEPENDS "${POLLO_HEX}")
add_dependencies(PollocadCore PolloStl)
