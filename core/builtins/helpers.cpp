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

gp_XYZ parseAnchor(const CallContext &c, const Value *pval, const gp_XYZ &default_) {
    if (!pval) {
        return gp_XYZ{};
    }

    if (const auto pstr = pval->as<std::string>()) {
        if (pstr->empty()) {
            return gp_XYZ{};
        }

        if (*pstr == "c") {
            return default_ * 0.5;
        }
    }

    return (default_ - parseDirection(c, "anchor", pval, gp_XYZ{})) * 0.5;
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

gp_XYZ parseVec(const CallContext &c, const std::string &name, const Value *pval, gp_XYZ default_) {
    if (!pval) {
        return default_;
    }

    auto plist = pval->as<List>();
    if (!plist) {
        c.warning(std::format("invalid {} (not a list): {}", name, pval->display()));
        return default_;
    }

    if (plist->size() > 3) {
        c.warning(std::format("invalid {} (excess elements: {})", name, plist->size()));
    }

    gp_XYZ result = default_;
    for (int i = 0; i < 3 && i < plist->size(); i++) {
        const auto &item = (*plist)[i];
        if (item.undefined()) {
            continue;
        } else if (const auto pnum = item.as<double>()) {
            result.SetCoord(i + 1, *pnum);
        } else {
            c.warning(std::format("invalid {} (contains non-numeric item): {}", name, pval->display()));
            return default_;
        }
    }

    return result;
}

gp_XYZ parseDirection(const CallContext &c, const std::string &name, const Value *pval, gp_XYZ default_) {
    if (!pval) {
        return default_;
    }

    gp_XYZ r{};
    if (pval->as<List>()) {
        r = parseVec(c, name, pval, default_);
    } else if (auto pdir = pval->as<std::string>()) {
        const auto &dir = *pdir;

        for (const char ch : dir) {
            switch (ch) {
                case 'l': r.SetX(-1.0); break;
                case 'r': r.SetX(+1.0); break;
                case 'n': r.SetY(-1.0); break;
                case 'f': r.SetY(+1.0); break;
                case 'b': case 'u': r.SetZ(-1.0); break;
                case 't': case 'd': r.SetZ(+1.0); break;
                default: c.warning(std::format("invalid {} (contains unknown character): '{}'", name, dir)); return default_;
            }
        }
    } else {
        c.warning(std::format("invalid {} (invalid type): {}", name, pval->display()));
        return default_;
    }

    return r;
}

gp_XYZ parseXYZ(const CallContext &c, double default_) {
    gp_XYZ vec{default_, default_, default_};

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

ShapeLocation parseShapeLocation(const CallContext &c, const gp_XYZ &defaultAnchor) {
    ShapeLocation loc;

    loc.anchor = parseAnchor(c, c.get("anchor"), defaultAnchor);

    if (const auto pspinVal = c.get("spin")) {
        if (const auto pspin = pspinVal->as<double>()) {
            loc.spin = *pspin;
        } else {
            c.warning(std::format("invalid spin (type is not number): {}", pspinVal->display()));
        }
    }

    auto porient = c.get("orient");
    auto orient = parseDirection(c, "orient", porient, c_xyzUp);
    if (!orient.IsEqual(gp_XYZ{}, Precision::Confusion())) {
        loc.orient = orient;
    } else {
        c.warning(std::format("invalid orient (magnitude is zero): {}", porient->display()));
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