#include "executor.h"

#include <BRepBndLib.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

namespace
{

TopoDS_Compound toCompound(const TaggedShapes& shapes) {
    TopoDS_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);

    for (const auto &sh : shapes) {
        builder.Add(comp, sh.shape);
    }

    return comp;
}

Bnd_Box getBoundingBox(const TaggedShapes& shapes) {
    Bnd_Box bbox;
    BRepBndLib::Add(toCompound(shapes), bbox);
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

Value addShapeChildren(const CallContext &c, TaggedShapes shape) {
    auto childrenp = c.get<Function>("$children");
    if (childrenp) {
        auto children = (**childrenp)(c.with("$parent", shape));
        if (children.error()) {
            return children;
        }

        if (auto childShapes = children.as<TaggedShapes>()) {
            std::move(childShapes->begin(), childShapes->end(), std::back_inserter(shape));
        } else {
            std::cerr << "Invalid children for shape";
        }
    }

    return shape;
}

}


Value builtin_box(const CallContext &c) {
    auto size = parseVec(c, 1.0);
    return addShapeChildren(c, TaggedShapes{BRepPrimAPI_MakeBox{size.X(), size.Y(), size.Z()}.Shape()});
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

    return addShapeChildren(c, TaggedShapes{BRepPrimAPI_MakeCylinder{r, h}.Shape()});
}

Value builtin_align(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto parentp = c.get<TaggedShapes>("$parent");
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

    TaggedShapes result;
    for (const auto &c : children) {
        result.emplace_back(c.shape.Moved(trsf), c.tags);
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
        trsf.SetRotation(gp_Ax1{{}, {1.0, 0.0, 0.0}}, v.X());
    }
    if (v.Y() != 0.0) {
        trsf.SetRotation(gp_Ax1{{}, {0.0, 1.0, 0.0}}, v.Y());
    }
    if (v.Z() != 0.0) {
        trsf.SetRotation(gp_Ax1{{}, {0.0, 0.0, 1.0}}, v.Z());
    }

    TaggedShapes result;
    for (const auto &c : children) {
        result.emplace_back(c.shape.Moved(trsf), c.tags);
    }

    return result;
}

Value builtin_tag(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto tag = c.get<std::string>(0);
    if (!tag) {
        return children;
    }

    TaggedShapes result;
    for (const auto &c : children) {
        std::unordered_set<std::string> new_tags = c.tags;
        new_tags.insert(*tag);
        result.emplace_back(c.shape, new_tags);
    }

    return result;
}

Value builtin_combine(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto remove = std::partition(children.begin(), children.end(), [](const auto& s) { return !s.tags.contains("remove"); });
    if (remove == children.begin()) {
        return undefined;
    }

    auto it = children.cbegin();
    TopoDS_Shape result = it->shape;
    it++;

    for (; it != remove; it++) {
        if (c.canceled()) {
            return undefined;
        }

        BRepAlgoAPI_Fuse fuse{result, it->shape};
        fuse.SimplifyResult();
        result = fuse.Shape();
    }

    for (; it != children.end(); it++) {
        if (c.canceled()) {
            return undefined;
        }

        BRepAlgoAPI_Cut cut{result, it->shape};
        cut.SimplifyResult();
        result = cut.Shape();
    }

    return TaggedShapes{result};
}


Value builtin_repeat(const CallContext &c) {
    const auto children = c.get<Function>("$children");
    if (!children) {
        return undefined;
    }

    const auto cv = c.get<double>(0);
    const int count = cv ? *cv : 0;

    TaggedShapes result;
    for (int i = 0; i < count; i++) {
        const auto value = (**children)(c.with("$i", static_cast<double>(i)));

        if (auto shapes = value.as<TaggedShapes>()) {
            std::move(shapes->begin(), shapes->end(), std::back_inserter(result));
        } else {
            return RuntimeError{"repeat only works on shapes"};
        }
    }

    return result;
}

void register_builtins_shapes(Environment &env) {
    env.add_function("box", builtin_box);
    env.add_function("align", builtin_align);
    env.add_function("cyl", builtin_cyl);
    env.add_function("move", builtin_move);
    env.add_function("rot", builtin_rot);
    env.add_function("tag", builtin_tag);
    env.add_function("combine", builtin_combine);
    env.add_function("repeat", builtin_repeat);
}


