#include <format>

#include "helpers.h"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <RWStl.hxx>
#include <TopoDS.hxx>

namespace
{

const uint8_t polloStl[] = {
#include "generated/pollo.hex"
};

Value addShapeChildren(const CallContext &c, ShapeList shape) {
    auto childrenp = c.get<Function>("$children");
    if (childrenp) {
        auto children = (*childrenp)(c.with("$parent", shape));
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
    static const auto defaultAnchor = gp_XYZ{-1.0, -1.0, -1.0};
    const auto size = parseXYZ(c, 1.0);
    const auto location = parseShapeLocation(c, defaultAnchor);

    if (size.X() <= Precision::Confusion() || size.Y() <= Precision::Confusion() || size.Z() <= Precision::Confusion()) {
        return undefined;
    }

    auto shape = BRepPrimAPI_MakeBox{gp_Pnt{}, size}.Shape();
    location.apply(shape, size);
    return addShapeChildren(c, ShapeList{Shape{shape, c.span()}});
}

Value builtin_cyl(const CallContext &c) {
    static const auto defaultAnchor = gp_XYZ{0.0, 0.0, -1.0};

    auto pr1 = c.get<double>("r1");
    auto pr2 = c.get<double>("r2");
    auto pd1 = c.get<double>("d1");
    auto pd2 = c.get<double>("d2");
    auto pr = c.get<double>("r");
    auto pd = c.get<double>("d");

    double r1 = pr1 ? *pr1 : pd1 ? *pd1 * 0.5 : pr ? *pr : pd ? *pd * 0.5 : 1.0;
    double r2 = pr2 ? *pr2 : pd2 ? *pd2 * 0.5 : pr ? *pr : pd ? *pd * 0.5 : 1.0;

    auto ph = c.get<double>("h");
    double h = ph ? *ph : 1.0;

    const auto location = parseShapeLocation(c, defaultAnchor);

    if ((r1 <= Precision::Confusion() && r2 <= Precision::Confusion()) || h <= Precision::Confusion()) {
        return undefined;
    }

    TopoDS_Shape shape;
    if (r1 == r2) {
        shape = BRepPrimAPI_MakeCylinder{r1, h}.Shape();
    } else {
        shape = BRepPrimAPI_MakeCone{r1, r2, h}.Shape();
    }

    double d = std::max(r1, r2) * 2.0;
    location.apply(shape, gp_XYZ{d, d, h});
    return addShapeChildren(c, ShapeList{Shape{shape, c.span()}});
}

Value builtin_sphere(const CallContext &c) {
    static const auto defaultAnchor = gp_XYZ{0.0, 0.0, 0.0};
    auto pr = c.get<double>("r");
    auto pd = c.get<double>("d");
    double r = pr ? *pr : pd ? *pd : 1.0;

    const auto location = parseShapeLocation(c, defaultAnchor);

    if (pr && pd) {
        c.warning("Both r and d defined for sphere - using r");
    }

    if (r <= Precision::Confusion()) {
        return undefined;
    }

    auto shape = BRepPrimAPI_MakeSphere{r}.Shape();
    location.apply(shape, gp_XYZ{r * 2.0, r * 2.0, r * 2.0});
    return addShapeChildren(c, ShapeList{Shape{shape, c.span()}});
}

TopoDS_Shape loadStlData(std::istream &s) {
    // No error checks here - data better be valid!

    s.ignore(80); // header

    uint32_t numTris = 0;
    s.read(reinterpret_cast<char *>(&numTris), sizeof(numTris)); // number of triangles

    struct VecF {
        float x, y, z;
    };

    TopoDS_Compound comp;
    TopoDS_Builder builder;
    builder.MakeCompound(comp);

    std::vector<TopoDS_Vertex> verts;

    for (uint32_t i = 0; i < numTris; i++) {
        s.ignore(sizeof(VecF)); // ignore normal

        size_t vertIndex[3];
        for (size_t j = 0; j < 3; j++) {
            float coords[3];
            s.read(reinterpret_cast<char *>(coords), sizeof(coords)); // vertices

            gp_Pnt pt{coords[0], coords[1], coords[2]};

            size_t index = 0;
            for (const auto &v : verts) {
                if (BRep_Tool::Pnt(v).IsEqual(pt, Precision::Approximation())) {
                    break;
                }

                index++;
            }

            if (index == verts.size()) {
                verts.push_back(BRepBuilderAPI_MakeVertex{pt});
            }

            vertIndex[j] = index;
        }

        builder.Add(comp, BRepBuilderAPI_MakeFace{
            BRepBuilderAPI_MakeWire{
                BRepBuilderAPI_MakeEdge{verts[vertIndex[0]], verts[vertIndex[1]]},
                BRepBuilderAPI_MakeEdge{verts[vertIndex[1]], verts[vertIndex[2]]},
                BRepBuilderAPI_MakeEdge{verts[vertIndex[2]], verts[vertIndex[0]]}
            }
        });

        s.ignore(sizeof(uint16_t)); // ignore "attribute byte count"
    }

    BRepBuilderAPI_Sewing sewing;
    sewing.Load(comp);
    sewing.Perform();

    return BRepBuilderAPI_MakeSolid{TopoDS::Shell(sewing.SewedShape())}.Shape();
}

TopoDS_Shape loadPollo() {
    std::istringstream s(std::string(reinterpret_cast<const char *>(polloStl), sizeof(polloStl)));
    return loadStlData(s);
}

Value builtin_pollo(const CallContext &c) {
    static const auto defaultAnchor = gp_XYZ{0.0, 0.0, -1.0};
    static auto pollo = loadPollo();
    const auto location = parseShapeLocation(c, defaultAnchor);

    auto shape = pollo;
    location.apply(shape, gp_XYZ{67.9, 124.11, 132.08});
    return addShapeChildren(c, ShapeList{Shape{shape, c.span()}});
}

}

void add_builtins_primitive_shapes(Environment &env) {
    env.setFunction("box", builtin_box);
    env.setFunction("cyl", builtin_cyl);
    env.setFunction("sphere", builtin_sphere);
    env.setFunction("pollo", builtin_pollo);
}


