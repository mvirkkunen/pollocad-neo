#include <format>

#include "contexts.h"

#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>

namespace
{

double degToRad(double deg) {
    return deg * (M_PI / 180.0);
}

TopoDS_Compound toCompound(const ShapeList& shapes) {
    TopoDS_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);

    for (const auto &sh : shapes) {
        builder.Add(comp, sh.shape());
    }

    return comp;
}

Bnd_Box getBoundingBox(const ShapeList& shapes) {
    Bnd_Box bbox;
    BRepBndLib::Add(toCompound(shapes), bbox);
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

Value builtin_align(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto parentp = c.get<ShapeList>("$parent");
    if (!parentp) {
        std::cerr << "No parent, cannot align";
        return children;
    }

    auto parentBounds = getBoundingBox(*parentp);
    auto childBounds = getBoundingBox(*parentp);

    return undefined;
}

Value builtin_move(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    gp_Trsf trsf;
    trsf.SetTranslation(parseVec(c, 0.0));

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

    auto v = parseVec(c, 0.0);

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
    for (const auto &c : children) {
        result.push_back(c.withShape(c.shape().Moved(trsf)));
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
        const auto value = (**children)(c.with(item));

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
        const auto plist = c.get<List>(0);
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

struct EdgeFilters {
    double r = 1.0;
    gp_XYZ dir{};
    gp_XYZ bound{0.5, 0.5, 0.5};
    Bnd_Box bbox{};
};

void parseEdgeSpec(const CallContext &c, std::vector<EdgeFilters> &out, double r, const Value &spec) {
    EdgeFilters filter{r};
    if (auto pstr = spec.as<std::string>()) {
        std::istringstream ss(*pstr);
        std::string specItem;
        while (ss >> specItem) {
            for (const auto ch : specItem) {
                switch (ch) {
                    case 'x': filter.dir.SetX(1.0); break;
                    case 'y': filter.dir.SetY(1.0); break;
                    case 'z': filter.dir.SetZ(1.0); break;
                    case 'r': filter.bound.SetX(1.0); break;
                    case 'f': filter.bound.SetY(1.0); break;
                    case 't': filter.bound.SetZ(1.0); break;
                    case 'l': filter.bound.SetX(0.0); break;
                    case 'n': filter.bound.SetY(0.0); break;
                    case 'b': filter.bound.SetZ(0.0); break;
                    default: c.warning(std::format("Invalid edge specification: {}", *pstr)); return;
                }
            }

            next:;
        }

        out.push_back(filter);
    } else if (auto pshape = spec.as<ShapeList>()) {
        filter.bbox = getBoundingBox(*pshape);
        out.push_back(filter);
    } else {
        c.warning(std::format("Invalid edge specification: {}", spec.display()));
    }
}

template <typename Algorithm>
Value builtin_chamfer_filler(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto pr = c.get<double>(1);
    double r = pr ? std::max(*pr, 0.0) : 1.0;

    auto plistOrSpec = c.get(0);
    if (!plistOrSpec) {
        return children;
    }

    std::vector<EdgeFilters> filters;

    if (auto plist = plistOrSpec->as<List>()) {
        for (const auto &spec : *plist) {
            if (const auto ppair = spec.as<List>()) {
                if (ppair->size() == 0 || ppair->size() > 2) {
                    c.warning(std::format("Invalid edge specification pair: {}", spec.display()));
                    continue;
                }

                auto pr = ppair->size() == 2 ? (*ppair)[1].as<double>() : &r;
                if (!pr) {
                    c.warning(std::format("Invalid radius specification: {}", (*ppair)[1].display()));
                    continue;
                }

                parseEdgeSpec(c, filters, *pr, (*ppair)[0]);
            } else {
                parseEdgeSpec(c, filters, r, spec);
            }
        }
    } else {
        parseEdgeSpec(c, filters, r, *plistOrSpec);
    }

    ShapeList result;
    for (const auto &ch : children) {
        Algorithm algo(ch.shape());
        auto shapeBoundingBox = getBoundingBox(ch.shape());
        bool anyMatch = false;

        for (TopExp_Explorer i(ch.shape(), TopAbs_EDGE); i.More(); i.Next()) {
            const auto &edge = TopoDS::Edge(i.Current());
            auto edgeBoundingBox = getBoundingBox(edge);
            bool match = false;

            for (const auto &f : filters) {
                if (!f.dir.IsEqual({}, 0.0)) {
                    auto dir = edgeBoundingBox.CornerMax().XYZ() - edgeBoundingBox.CornerMin().XYZ();
                    const auto dist = dir.Modulus();
                    if (dist < gp::Resolution()) {
                        continue;
                    }

                    dir /= dist;

                    if (!(dir.IsEqual(f.dir, Precision::Approximation()) || dir.IsEqual(f.dir.Reversed(), Precision::Approximation()))) {
                        continue;
                    }
                }

                if (!f.bound.IsEqual({0.5, 0.5, 0.5}, 0.0)) {
                    const auto min = shapeBoundingBox.CornerMin().XYZ();
                    const auto max = shapeBoundingBox.CornerMax().XYZ();
                    const auto pt = min + (max - min).Multiplied(f.bound);
                    const auto plane = gp_Pln{gp_Ax3{pt, f.bound - gp_XYZ{0.5, 0.5, 0.5}}};

                    if (!(plane.Contains(edgeBoundingBox.CornerMin(), Precision::Approximation()) && plane.Contains(edgeBoundingBox.CornerMax(), Precision::Approximation()))) {
                        continue;
                    }
                }

                if (!f.bbox.IsVoid()) {
                    if (!f.bbox.IsOut(edgeBoundingBox)) {
                        continue;
                    }
                }

                match = true;
                break;
            }

            if (match && r > 0.0) {
                anyMatch = true;
                algo.Add(r, edge);
            }
        }

        if (!anyMatch) {
            c.warning(std::format("No edges found to process"));
            return children;
        }

        algo.Build();
        if (!algo.IsDone()) {
            c.error(std::format("Operation failed. Shape is too complex or radius is too large."));
            result.push_back(ch);
            continue;
        }

        result.push_back(ch.withShape(algo.Shape(), c.span()));
    }

    return result;
}

void register_builtins_shapes(Environment &env) {
    env.setFunction("box", builtin_box);
    env.setFunction("align", builtin_align);
    env.setFunction("cyl", builtin_cyl);
    env.setFunction("move", builtin_move);
    env.setFunction("rot", builtin_rot);
    env.setFunction("tag", builtin_tag);
    env.setFunction("remove", builtin_remove);
    env.setFunction("prop", builtin_prop);
    env.setFunction("combine", builtin_combine);
    env.setFunction("for", builtin_for);
    env.setFunction("chamfer", builtin_chamfer_filler<BRepFilletAPI_MakeChamfer>);
    env.setFunction("fillet", builtin_chamfer_filler<BRepFilletAPI_MakeFillet>);
}


