#include <format>

#include <BRepBndLib.hxx>
#include <gp_Ax1.hxx>
#include <gp_Quaternion.hxx>
#include <Precision.hxx>
#include <TopoDS_Shape.hxx>

#include "helpers.h"
#include "contexts.h"

namespace
{

gp_XYZ parseAnchor(const Argument &arg, const gp_XYZ &default_) {
    if (!arg) {
        return gp_XYZ{};
    }

    if (arg.is<std::string>()) {
        auto str = arg.as<std::string>();
        if (str.empty()) {
            return gp_XYZ{};
        }

        if (str == "c") {
            return default_ * 0.5;
        }
    }

    return (default_ - parseDirection(arg, gp_XYZ{})) * 0.5;
}

}

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

gp_XYZ parseVec(const Argument &arg, gp_XYZ default_, int elements) {
    if (!arg) {
        return default_;
    }

    auto list = arg.as<ValueList>();
    if (list.empty()) {
        return default_;
    }

    if (list.size() > elements) {
        arg.warning("excess elements, expected {}, got {}", elements, list.size());
    }

    gp_XYZ result = default_;
    for (int i = 0; i < 3 && i < list.size(); i++) {
        const auto &item = list.at(i);
        if (item.isUndefined()) {
            continue;
        } else if (item.is<double>()) {
            result.SetCoord(i + 1, item.as<double>());
        } else {
            arg.error("contains non-numeric item: {}", item.display());
            return default_;
        }
    }

    return result;
}

gp_XYZ parseDirection(const Argument &arg, gp_XYZ default_) {
    gp_XYZ r = default_;
    arg.overload(
        [&](const ValueList &list) {
            r = parseVec(arg, default_);
        },
        [&](const std::string &str) { // TODO: const &
            for (const char ch : str) {
                switch (ch) {
                    case 'l': r.SetX(-1.0); break;
                    case 'r': r.SetX(+1.0); break;
                    case 'n': r.SetY(-1.0); break;
                    case 'f': r.SetY(+1.0); break;
                    case 'b': case 'd': r.SetZ(-1.0); break;
                    case 't': case 'u': r.SetZ(+1.0); break;
                    default: arg.warning("contains unknown character: '{}'", ch); break;
                }
            }

            out:;
        },
        [&](const Undefined &) { }
    );

    return r;
}

gp_XYZ parseXYZ(const CallContext &c, const Argument &arg, double default_) {
    gp_XYZ vec{default_, default_, default_};

    if (arg) {
        vec = parseVec(arg);
    }

    auto x = c.named("x");
    if (x) {
        vec.SetX(x.as<double>());
    }

    auto y = c.named("y");
    if (y) {
        vec.SetY(y.as<double>());
    }

    auto z = c.named("z");
    if (z) {
        vec.SetZ(z.as<double>());
    }

    return vec;
}

gp_XY parseXY(const CallContext &c, const Argument &arg, double default_) {
    gp_XY vec{default_, default_};

    if (arg) {
        auto xy = parseVec(arg, {default_, default_, default_}, 2);
        vec.SetX(xy.X());
        vec.SetY(xy.Y());
    }

    auto x = c.named("x");
    if (x) {
        vec.SetX(x.as<double>());
    }

    auto y = c.named("y");
    if (y) {
        vec.SetY(y.as<double>());
    }

    return vec;
}

ShapeLocation parseShapeLocation(const CallContext &c, const gp_XYZ &defaultAnchor) {
    ShapeLocation loc;

    loc.anchor = parseAnchor(c.named("anchor"), defaultAnchor);

    auto aspin = c.named("spin");
    if (aspin) {
        loc.spin = aspin.as<double>();
    }

    auto aorient = c.named("orient");
    if (aorient) {
        auto orient = parseDirection(aorient, c_xyzUp);
        if (!orient.IsEqual(gp_XYZ{}, Precision::Confusion())) {
            loc.orient = orient;
        } else {
            aorient.warning("magnitude is zero");
        }
    }

    return loc;
}

void ShapeLocation::apply(TopoDS_Shape &shape, const gp_XYZ &size) const {
    gp_Trsf location;
    bool transform = false;

    if (!orient.IsEqual(c_xyzUp, Precision::Confusion())) {
        transform = true;
        gp_Trsf tr;
        tr.SetRotation(gp_Quaternion{c_xyzUp, orient});
        location.Multiply(tr);
    }

    if (std::fmod(spin, 360.0) > Precision::Confusion()) {
        transform = true;
        gp_Trsf tr;
        tr.SetRotation(gp_Ax1{gp_Pnt{}, c_xyzUp}, degToRad(spin));
        location.Multiply(tr);
    }

    if (!anchor.IsEqual(c_xyzZero, Precision::Confusion())) {
        transform = true;
        gp_Trsf tr;
        tr.SetTranslation(anchor.Multiplied(size));
        location.Multiply(tr);
    }

    if (transform) {
        shape.Move(location);
    }
}