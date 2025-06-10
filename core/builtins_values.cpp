#include "executor.h"

class BuiltinIf : public FunctionImpl {
public:
    Value call(const CallContext &c) override {
        if (c.count() < 2) {
            return RuntimeError{"Malformed if clause (too few arguments)"};
        }

        if (c.get(0).truthy()) {
            auto then = c.get<Function>(1);
            if (!then) {
                return RuntimeError{"Invalid then block in if clause"};
            }

            return (*then)->call(c.sub());
        }

        if (c.canceled()) {
            return undefined;
        }

        if (c.count() == 3) {
            auto else_ = c.get<Function>(2);
            if (!else_) {
                return RuntimeError{"Invalid else block in if clause"};
            }

            return (*else_)->call(c.sub());
        }

        return RuntimeError{"Malformed if clause (too many arguments)"};
    }
};

class BuiltinPlus : public FunctionImpl {
public:
    Value call(const CallContext &c) override {
        double result = 0.0;

        for (const Value &v : c.positional()) {
            auto n = v.as<double>();
            if (n) {
                result += *n;
            }
        }

        return result;
    }
};

class BuiltinList : public FunctionImpl {
public:
    Value call(const CallContext &c) override {
        return c.positional();
    }
};

void register_builtins_values(Environment &env) {
    env.vars.insert({"if", std::make_shared<BuiltinIf>()});
    env.vars.insert({"+", std::make_shared<BuiltinPlus>()});
    env.vars.insert({"list", std::make_shared<BuiltinList>()});
}
