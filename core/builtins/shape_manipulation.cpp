#include <format>

#include "helpers.h"

#include <gp_Ax1.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <TopoDS.hxx>

namespace
{

Value builtin_move(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    gp_Trsf trsf;
    trsf.SetTranslation(parseXYZ(c, 0.0));

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withShape(c.shape().Moved(trsf)));
    }

    return result;
}

Value builtin_rot(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto v = parseXYZ(c, 0.0);

    gp_Trsf trsf;
    if (v.X() != 0.0) {
        trsf.SetRotation(gp_Ax1{{}, {1.0, 0.0, 0.0}}, degToRad(v.X()));
    }
    if (v.Y() != 0.0) {
        trsf.SetRotation(gp_Ax1{{}, {0.0, 1.0, 0.0}}, degToRad(v.Y()));
    }
    if (v.Z() != 0.0) {
        trsf.SetRotation(gp_Ax1{{}, {0.0, 0.0, 1.0}}, degToRad(v.Z()));
    }

    ShapeList result;
    for (const auto &ch : children) {
        result.push_back(ch.withShape(ch.shape().Moved(trsf)));
    }

    return result;
}

Value builtin_orient(const CallContext &c) {
    const auto location = parseShapeLocation(c, gp_XYZ{-1.0, -1.0, -1.0});

    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto bbox = getBoundingBox(children);

    ShapeList result;
    for (const auto &ch : children) {
        TopoDS_Shape shape = ch.shape();
        location.apply(shape, bbox.CornerMax().XYZ() - bbox.CornerMin().XYZ());
        result.push_back(ch.withShape(shape));
    }

    return result;
}

Value builtin_tag(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto ptag = c.get<std::string>(0);
    if (!ptag) {
        return children;
    }

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withProp(*ptag, true));
    }

    return result;
}

Value builtin_remove(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withProp("remove", true));
    }

    return result;
}

Value builtin_prop(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto pname = c.get<std::string>(0);
    if (!pname) {
        return children;
    }

    auto pvalue = c.get(1);
    if (!pvalue) {
        return children;
    }

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withProp(*pname, *pvalue));
    }

    return result;
}

Value builtin_combine(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    ShapeList result;
    std::copy_if(
        children.begin(),
        children.end(),
        std::back_inserter(result),
        [](const auto &ch) { return ch.hasProp("highlight") && ch.hasProp("remove"); }
    );
    
    auto remove = std::partition(children.begin(), children.end(), [](const auto& s) { return !s.hasProp("remove"); });
    if (remove == children.begin()) {
        return undefined;
    }

    std::vector<Span> spans;

    auto it = children.cbegin();
    TopoDS_Shape shape = it->shape();
    std::copy(it->spans().begin(), it->spans().end(), std::back_inserter(spans));
    it++;

    for (; it != remove; it++) {
        if (c.canceled()) {
            return undefined;
        }

        std::copy(it->spans().begin(), it->spans().end(), std::back_inserter(spans));

        BRepAlgoAPI_Fuse fuse{shape, it->shape()};
        fuse.SimplifyResult();
        shape = fuse.Shape();
    }

    for (; it != children.end(); it++) {
        if (c.canceled()) {
            return undefined;
        }

        BRepAlgoAPI_Cut cut{shape, it->shape()};
        cut.SimplifyResult();
        shape = cut.Shape();
    }

    result.push_back(Shape{shape, spans});
    return result;
}

Value builtin_for(const CallContext &c) {
    const auto children = c.get<Function>("$children");
    if (!children) {
        return undefined;
    }

    ShapeList result;
    const auto iteration = [&](const Value &item) {
        const auto value = (*children)(c.with(item));

        if (value.undefined()) {
            // ignore
        } else if (auto shapes = value.as<ShapeList>()) {
            std::move(shapes->begin(), shapes->end(), std::back_inserter(result));
        } else {
            c.error("for children must be shapes");
            return false;
        }

        return true;
    };

    if (c.positional().size() == 1) {
        const auto plist = c.get<ValueList>(0);
        if (!plist) {
            return c.error("attempted to for loop over something that is not a list");
        }

        for (const auto &item: *plist) {
            if (c.canceled()) {
                return undefined;
            }

            if (!iteration(item)) {
                return undefined;
            }
        }
    } else if (c.positional().size() <= 3) {
        const auto pfrom = c.get<double>(0);
        if (!pfrom) {
            return c.error("for loop start value must be a number");
        }

        const auto pto = c.get<double>(c.positional().size() - 1);
        if (!pto) {
            return c.error("for loop to value must be a number");
        }

        constexpr double defaultStep = 1.0;
        const auto pstep = c.positional().size() == 3 ? c.get<double>(1) : &defaultStep;
        if (!pstep) {
            return c.error("for loop step value must be a number");
        }

        double from = *pfrom, step = *pstep, to = *pto;

        if (step == 0.0) {
            return c.error("for loop step value cannot be zero");
        }

        if (*pto < *pfrom && step > 0) {
            step = -step;
        }

        for (double i = from; i <= to; i += step) {
            if (c.canceled()) {
                return undefined;
            }

            if (!iteration(i)) {
                return undefined;
            }
        }
    } else {
        return c.error("malformed for loop (too many arguments)");
    }

    return result;
}

Value builtin_bounds(const CallContext &c) {
    Bnd_Box result;

    result.Add(getBoundingBox(c.children()));
    
    for (const auto &arg : c.positional()) {
        if (const auto pshape = arg.as<ShapeList>()) {
            result.Add(getBoundingBox(*pshape));
        }
    }

    return ValueList{
        ValueList{
            result.CornerMin().X(),
            result.CornerMin().Y(),
            result.CornerMin().Z(),
        },
        ValueList{
            result.CornerMax().X(),
            result.CornerMax().Y(),
            result.CornerMax().Z(),
        },
    };
}

}

void add_builtins_shape_manipulation(Environment &env) {
    env.setFunction("move", builtin_move);
    env.setFunction("rot", builtin_rot);
    env.setFunction("orient", builtin_orient);
    env.setFunction("tag", builtin_tag);
    env.setFunction("remove", builtin_remove);
    env.setFunction("prop", builtin_prop);
    env.setFunction("combine", builtin_combine);
    env.setFunction("for", builtin_for);
    env.setFunction("bounds", builtin_bounds);
}


