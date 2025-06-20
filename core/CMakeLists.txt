add_library(PollocadCore
    parser.h parser.cpp
    ast.h ast.cpp
    value.h value.cpp
    contexts.cpp contexts.h
    executor.h executor.cpp
    logmessage.h
    builtins/helpers.h builtins/helpers.cpp
    builtins/chamfer_fillet.cpp
    builtins/primitive_shapes.cpp
    builtins/primitives.cpp
    builtins/shape_manipulation.cpp
)

target_include_directories(PollocadCore
    PUBLIC
        ../lexy/include
        /home/matti/devel/OCCT/build/include/opencascade
        .)

target_link_directories(PollocadCore PUBLIC /home/matti/devel/OCCT/build/lin64/gcc/libi)

target_link_libraries(PollocadCore
    PUBLIC
        TKBO
        TKBRep
        TKernel
        TKFillet
        TKMath
        TKOpenGl
        TKPrim
        TKService
        TKTopAlgo
        TKV3d
)
