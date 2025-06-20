#include <BRepBndLib.hxx>
#include <TopoDS_Shape.hxx>

#include "helpers.h"
#include "contexts.h"


Bnd_Box getBoundingBox(const ShapeList& shapes) {
    Bnd_Box bbox;
    for (const auto &sh : shapes) {
        BRepBndLib::Add(sh.shape(), bbox);
    }
    return bbox;
}

Bnd_Box getBoundingBox(const TopoDS_Shape& shape) {
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    return bbox;
}

gp_Vec parseVec(const CallContext &c, double default_) {
    gp_Vec vec{default_, default_, default_};

    if (auto l = c.get<List>(0)) {
        for (int i = 0; i < l->size() && i < 3; i++) {
            if (auto n = (*l)[i].as<double>()) {
                vec.SetCoord(i + 1, *n);
            }
        }
    }

    if (auto n = c.get<double>("x")) {
        vec.SetX(*n);
    }

    if (auto n = c.get<double>("y")) {
        vec.SetY(*n);
    }

    if (auto n = c.get<double>("z")) {
        vec.SetZ(*n);
    }

    return vec;
}