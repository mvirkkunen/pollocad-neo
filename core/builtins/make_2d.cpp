#include <format>

#include "helpers.h"

#include <gp_Circ.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>

#include <RWStl.hxx>
#include <TopoDS.hxx>

namespace
{

Value builtin_rect(const CallContext &c) {
    static const auto defaultAnchor = gp_XYZ{-1.0, -1.0, 0.0};
    const auto size = parseXY(c, 1.0);
    const auto location = parseShapeLocation(c, defaultAnchor);

    if (size.X() <= Precision::Confusion() || size.Y() <= Precision::Confusion()) {
        return undefined;
    }

    auto p0 = gp_Pnt{0.0, 0.0, 0.0};
    auto p1 = gp_Pnt{size.X(), 0.0, 0.0};
    auto p2 = gp_Pnt{size.X(), size.Y(), 0.0};
    auto p3 = gp_Pnt{0.0, size.Y(), 0.0};

    auto wire = BRepBuilderAPI_MakeWire{
        BRepBuilderAPI_MakeEdge{p0, p1},
        BRepBuilderAPI_MakeEdge{p1, p2},
        BRepBuilderAPI_MakeEdge{p2, p3},
        BRepBuilderAPI_MakeEdge{p3, p0},
    };

    bool asFace = true;
    if (auto pwire = c.get("wire")) {
        asFace = !pwire->truthy();
    }

    auto shape = asFace ? BRepBuilderAPI_MakeFace{wire.Wire()}.Shape() : wire.Shape();

    location.apply(shape, gp_XYZ{size.X(), size.Y(), 0.0});
    return ShapeList{Shape{shape, c.span()}};
}

Value builtin_circ(const CallContext &c) {
    static const auto defaultAnchor = gp_XYZ{0.0, 0.0, 0.0};

    auto pr = c.get<double>("r");
    auto pd = c.get<double>("d");

    auto r = pr ? *pr : pd ? *pd * 0.5 : 1.0;

    const auto location = parseShapeLocation(c, defaultAnchor);

    if (r <= Precision::Confusion()) {
        return undefined;
    }

    auto wire = BRepBuilderAPI_MakeWire{BRepBuilderAPI_MakeEdge{gp_Circ{gp_Ax2{}, r}}};

    bool asFace = true;
    if (auto pwire = c.get("wire")) {
        asFace = !pwire->truthy();
    }

    auto shape = asFace ? BRepBuilderAPI_MakeFace{wire.Wire()}.Shape() : wire.Shape();

    location.apply(shape, gp_XYZ{r * 2.0, r * 2.0, 0.0});
    return ShapeList{Shape{shape, c.span()}};
}

}

void add_builtins_make_2d(Environment &env) {
    env.setFunction("rect", builtin_rect);
    env.setFunction("circ", builtin_circ);
}


