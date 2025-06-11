

qt_add_library(PollocadCore
    ast.h ast.cpp
    executor.h executor.cpp
    parser.h parser.cpp
    builtins_values.cpp

    builtins_shapes.cpp
    value.h value.cpp
)

target_include_directories(PollocadCore
    PUBLIC
        ../lexy/include
        /home/matti/devel/OCCT/build/include/opencascade
        .)

target_link_directories(PollocadCore PUBLIC /home/matti/devel/OCCT/build/lin64/gcc/libi)

target_link_libraries(PollocadCore
    PUBLIC
        Qt6::Core
        TKBO
        TKBRep
        TKernel
        TKMath
        TKOpenGl
        TKPrim
        TKService
        TKTopAlgo
        TKV3d
)
