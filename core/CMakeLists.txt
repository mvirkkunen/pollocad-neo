add_library(PollocadCore
    parser.h parser.cpp
    ast.h ast.cpp
    value.h value.cpp
    contexts.cpp contexts.h
    executor.h executor.cpp
    logmessage.h
    builtins_values.cpp
    builtins_shapes.cpp
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
        TKMath
        TKOpenGl
        TKPrim
        TKService
        TKTopAlgo
        TKV3d
)
