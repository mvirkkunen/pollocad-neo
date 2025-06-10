#include "executor.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>

class BuiltinBox : public FunctionImpl {
public:
    Value call(const CallContext &c) override {
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
};

class BuiltinMove : public FunctionImpl {
public:
    Value call(const CallContext &c) override {
        auto children = c.children();
        if (children.empty()) {
            std::cerr << "no children :(";
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

        TaggedShapes results;
        for (const auto &c : children) {
            results.emplace_back(c.shape.Moved(trsf), c.tags);
        }

        return results;
    }
};

void register_builtins_shapes(Environment &env) {
    env.vars.insert({"box", std::make_shared<BuiltinBox>()});
    env.vars.insert({"move", std::make_shared<BuiltinMove>()});
}


