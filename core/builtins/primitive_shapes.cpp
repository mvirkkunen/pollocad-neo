#include "helpers.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS.hxx>

namespace
{

Value addShapeChildren(const CallContext &c, ShapeList shape) {
    auto childrenp = c.get<Function>("$children");
    if (childrenp) {
        auto children = (**childrenp)(c.with("$parent", shape));
        // TODO FIXME
        /*if (children.error()) {
            return undefined;
        }*/

        if (children.undefined()) {
            return shape;
        }

        if (auto childShapes = children.as<ShapeList>()) {
            std::move(childShapes->begin(), childShapes->end(), std::back_inserter(shape));
        } else {
            return c.error("Invalid children for shape");
        }
    }

    return shape;
}

Value builtin_box(const CallContext &c) {
    auto size = parseVec(c, 1.0);
    return addShapeChildren(c, ShapeList{Shape{BRepPrimAPI_MakeBox{size.X(), size.Y(), size.Z()}.Shape(), c.span()}});
}

Value builtin_cyl(const CallContext &c) {
    auto pr = c.get<double>("r");
    auto pd = c.get<double>("d");
    double r = pr ? *pr : pd ? *pd : 1.0;

    auto ph = c.get<double>("h");
    double h = ph ? *ph : 1.0;

    if (r <= 0.0 || h <= 0.0) {
        return undefined;
    }

    return addShapeChildren(c, ShapeList{Shape{BRepPrimAPI_MakeCylinder{r, h}.Shape(), c.span()}});
}

}

void add_builtins_primitive_shapes(Environment &env) {
    env.setFunction("box", builtin_box);
    env.setFunction("cyl", builtin_cyl);
}


