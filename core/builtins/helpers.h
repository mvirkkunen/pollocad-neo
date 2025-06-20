#pragma once

#include <Bnd_Box.hxx>
#include <gp_Vec.hxx>

#include "contexts.h"

class CallContext;

Bnd_Box getBoundingBox(const ShapeList& shapes);
Bnd_Box getBoundingBox(const TopoDS_Shape& shape);
gp_Vec parseVec(const CallContext &c, double default_);

inline double degToRad(double deg) {
    return deg * (M_PI / 180.0);
}
