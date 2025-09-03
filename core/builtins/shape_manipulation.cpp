#include <format>

#include "helpers.h"

#include <gp_Ax1.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepTools.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Builder.hxx>
#include <TopoDS_Compound.hxx>

namespace
{

Value builtin_move(CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    gp_Trsf trsf;
    trsf.SetTranslation(parseXYZ(c, c.arg("position"), 0.0));

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withShape(c.shape().Moved(trsf)));
    }

    return result;
}

Value builtin_rot(CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto v = parseXYZ(c, c.arg("rotation"), 0.0);

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

Value builtin_tag(CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto tag = c.arg("tag").as<std::string>();
    if (tag.empty()) {
        return children;
    }

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withProp(tag, true));
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

Value builtin_prop(CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto name = c.arg("name").as<std::string>();
    auto value = c.arg("value").asAny();

    if (name.empty() || !value) {
        return children;
    }

    ShapeList result;
    for (const auto &c : children) {
        result.push_back(c.withProp(name, value));
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

    TopoDS_Shape cutShape;
    for (; it != children.end(); it++) {
        if (c.canceled()) {
            return undefined;
        }

        if (cutShape.IsNull()) {
            cutShape = it->shape();
        } else {
            BRepAlgoAPI_Fuse fuse{cutShape, it->shape()};
            fuse.SimplifyResult();
            cutShape = fuse.Shape();
        }
    }

    if (!cutShape.IsNull()) {
        BRepAlgoAPI_Cut cut{shape, cutShape};
        cut.SimplifyResult();
        shape = cut.Shape();
    }

    result.push_back(Shape{shape, spans});
    return result;
}

Value builtin_for(CallContext &c) {
    const auto achildren = c.named("$children");
    if (!achildren) {
        return undefined;
    }

    auto children = achildren.as<Function>();

    ShapeList result;
    const auto iteration = [&](const Value &item) {
        auto cc = c.with(item);
        auto value = children(cc);

        if (!value) {
            // ignore
        } else if (value.is<ShapeList>()) {
            auto shapes = value.as<ShapeList>();
            std::move(shapes.cbegin(), shapes.cend(), std::back_inserter(result));
        } else {
            c.error("for children must be shapes");
            result.clear();
            return false;
        }

        return true;
    };

    size_t argCount = c.allPositional().size();

    if (argCount == 1) {
        c.arg("iterable").overload([&](ValueList list) {
            for (const auto &item: list) {
                if (c.canceled()) {
                    return;
                }

                if (!iteration(item)) {
                    return;
                }
            }
        });
    } else if (argCount <= 3) {
        const auto afrom = c.allPositional().at(0);
        if (!afrom.is<double>()) {
            return c.error("for loop start value must be a number");
        }
        double from = afrom.as<double>();

        const auto ato = c.allPositional().at(argCount - 1);
        if (!ato.is<double>()) {
            return c.error("for loop to value must be a number");
        }
        double to = ato.as<double>();

        double step = 1.0;
        if (argCount == 3) {
            const auto astep = c.allPositional().at(1);
            if (!astep.is<double>()) {
                return c.error("for loop step value must be a number");
            }
            step = astep.as<double>();
        }

        if (step == 0.0) {
            return c.error("for loop step value cannot be zero");
        }

        if (to < from && step > 0) {
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

Value builtin_thru_sections(const CallContext &c) {
    auto children = c.children();
    if (children.empty()) {
        return undefined;
    }

    auto algo = BRepOffsetAPI_ThruSections{true, true};

    for (const auto &ch : children) {
        auto shape = ch.shape();

        for (TopExp_Explorer i(ch.shape(), TopAbs_WIRE); i.More(); i.Next()) {
            algo.AddWire(TopoDS::Wire(i.Current()));
        }

        /*switch (shape.ShapeType()) {
            case TopAbs_FACE: {
                auto wire = BRepTools::OuterWire(TopoDS::Face(shape));
                if (wire.IsNull()) {
                    c.warning("ignoring child shape: face does not have an outer wire");
                    break;
                }
                algo.AddWire(wire);
                break;
            }

            case TopAbs_WIRE:
                algo.AddWire(TopoDS::Wire(shape));
                break;

            case TopAbs_VERTEX:
                algo.AddVertex(TopoDS::Vertex(shape));
                break;

            default:
                c.warning("invalid shape type for thru_sections: {}", (int)shape.ShapeType());
                break;
        }*/
    }

    return ShapeList{algo.Shape()};
}

Value builtin_bounds(const CallContext &c) {
    Bnd_Box result;

    result.Add(getBoundingBox(c.children()));
    
    for (const auto &arg : c.allPositional()) {
        if (arg.is<ShapeList>()) {
            result.Add(getBoundingBox(arg.as<ShapeList>()));
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
    env.setFunction("thru_sections", builtin_thru_sections);
    env.setFunction("bounds", builtin_bounds);
}


