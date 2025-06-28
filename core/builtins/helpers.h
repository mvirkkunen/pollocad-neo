#pragma once

#include <Bnd_Box.hxx>
#include <gp_Vec.hxx>

#include "contexts.h"

const gp_XYZ c_xyzZero{0.0, 0.0, 0.0};
const gp_XYZ c_xyzUp{0.0, 0.0, 1.0};

class CallContext;

struct ShapeLocation {
    gp_XYZ anchor = c_xyzZero;
    gp_XYZ orient = c_xyzUp;
    double spin = 0.0;

    void apply(TopoDS_Shape &shape, const gp_XYZ &size) const;
};

Bnd_Box getBoundingBox(const ShapeList& shapes);
Bnd_Box getBoundingBox(const TopoDS_Shape& shape);
gp_XYZ parseXYZ(const CallContext &c, const Argument &arg, double default_);
gp_XY parseXY(const CallContext &c, const Argument &arg, double default_);
gp_XYZ parseVec(const Argument &arg, gp_XYZ default_={}, int elements=3);
gp_XYZ parseDirection(const Argument &arg, gp_XYZ default_={});

inline double degToRad(double deg) {
    return deg * (M_PI / 180.0);
}

ShapeLocation parseShapeLocation(const CallContext &c, const gp_XYZ &defaultAnchor);
