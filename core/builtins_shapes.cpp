#include "executor.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

Value builtin_box(const CallContext &c) {
    double size[3] = { 1.0, 1.0, 1.0 };

    if (c.count() > 0) {
        if (auto l = c.get<List>(0)) {
            for (int i = 0; i < l->size() && i < 3; i++) {
                if (auto n = (*l)[i].as<double>()) {
                    size[i] = *n;
                }
            }
        }
    }

    if (size[0] == 0.0 || size[1] == 0.0 || size[2] == 0.0) {
        return undefined;
    }

    return TaggedShapes{BRepPrimAPI_MakeBox{size[0], size[1], size[2]}.Shape()};
}

Value builtin_cyl(const CallContext &c) {
    double size[3] = { 1.0, 1.0, 1.0 };

    auto rv = c.get("r").as<double>();
    double r = rv && *rv > 0 ? *rv : 1.0;

    auto hv = c.get("h").as<double>();
    double h = hv && *hv > 0 ? *hv : 1.0;

    return TaggedShapes{BRepPrimAPI_MakeCylinder{r, h}.Shape()};
}

Value builtin_move(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    gp_Vec loc;
    if (c.count() > 0) {
        if (auto l = c.get<List>(0)) {
            for (int i = 0; i < l->size() && i < 3; i++) {
                if (auto n = (*l)[i].as<double>()) {
                    loc.SetCoord(i + 1, *n);
                }
            }
        }
    }

    gp_Trsf trsf;
    trsf.SetTranslation(loc);

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
    env.add_function("cyl", builtin_cyl);
    env.add_function("move", builtin_move);
    env.add_function("tag", builtin_tag);
    env.add_function("combine", builtin_combine);
    env.add_function("repeat", builtin_repeat);
}


