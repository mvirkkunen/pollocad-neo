#include <format>

#include "helpers.h"

#include <BRepBndLib.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>

namespace
{

struct EdgeFilters {
    double r = 1.0;
    gp_XYZ dir{};
    gp_XYZ bound{0.5, 0.5, 0.5};
    Bnd_Box bbox{};
    TopoDS_Shape shape;
};

void parseEdgeSpec(const CallContext &c, std::vector<Shape> &highlightOut, std::vector<EdgeFilters> &out, double r, const Value &spec) {
    if (auto pstr = spec.as<std::string>()) {
        std::istringstream ss(*pstr);
        std::string specItem;
        while (ss >> specItem) {
            EdgeFilters filter{r};
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

            out.push_back(filter);

            next:;
        }
    } else if (auto pshape = spec.as<ShapeList>()) {
        EdgeFilters filter{r};
        filter.bbox = getBoundingBox(*pshape);
        out.push_back(filter);
        std::copy_if(pshape->begin(), pshape->end(), std::back_inserter(highlightOut), [](const Shape &sh) { return sh.hasProp("highlight"); });
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

    ShapeList result;
    std::vector<EdgeFilters> filters;

    if (auto plist = plistOrSpec->as<ValueList>()) {
        for (const auto &spec : *plist) {
            if (const auto ppair = spec.as<ValueList>()) {
                if (ppair->size() == 0 || ppair->size() > 2) {
                    c.warning(std::format("Invalid edge specification pair: {}", spec.display()));
                    continue;
                }

                auto pr = ppair->size() == 2 ? (*ppair)[1].as<double>() : &r;
                if (!pr) {
                    c.warning(std::format("Invalid radius specification: {}", (*ppair)[1].display()));
                    continue;
                }

                parseEdgeSpec(c, result, filters, *pr, (*ppair)[0]);
            } else {
                parseEdgeSpec(c, result, filters, r, spec);
            }
        }
    } else {
        parseEdgeSpec(c, result, filters, r, *plistOrSpec);
    }

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
                    if (!(
                        edgeBoundingBox.CornerMin().X() >= f.bbox.CornerMin().X()
                        && edgeBoundingBox.CornerMin().Y() >= f.bbox.CornerMin().Y()
                        && edgeBoundingBox.CornerMin().Z() >= f.bbox.CornerMin().Z()
                        && edgeBoundingBox.CornerMax().X() <= f.bbox.CornerMax().X()
                        && edgeBoundingBox.CornerMax().Y() <= f.bbox.CornerMax().Y()
                        && edgeBoundingBox.CornerMax().Z() <= f.bbox.CornerMax().Z()))
                    {
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

}

void add_builtins_chamfer_fillet(Environment &env) {
    env.setFunction("chamfer", builtin_chamfer_filler<BRepFilletAPI_MakeChamfer>);
    env.setFunction("fillet", builtin_chamfer_filler<BRepFilletAPI_MakeFillet>);
}
