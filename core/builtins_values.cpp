#include "executor.h"

Value builtin_if(const CallContext &c) {
    if (c.count() < 2) {
        return RuntimeError{"Malformed if clause (too few arguments)"};
    }

    if (c.get(0).truthy()) {
        auto then = c.get<Function>(1);
        if (!then) {
            return RuntimeError{"Invalid then block in if clause"};
        }

        return (**then)(c.empty());
    }

    if (c.canceled()) {
        return undefined;
    }

    if (c.count() == 3) {
        auto else_ = c.get<Function>(2);
        if (!else_) {
            return RuntimeError{"Invalid else block in if clause"};
        }

        return (**else_)(c.empty());
    }

    return RuntimeError{"Malformed if clause (too many arguments)"};
}

Value builtin_minus(const CallContext &c) {
    double result = 0.0;

    for (const Value &v : c.positional()) {
        auto n = v.as<double>();
        if (n) {
            result -= *n;
        }
    }

    return result;
}

auto builtin_bin_op(std::function<double(double, double)> op) {
    return [op](const CallContext &c) -> Value {
        auto it = c.positional().begin();
        if (it == c.positional().end()) {
            return undefined;
        }

        auto n = it->as<double>();
        if (!n) {
            return undefined;
        }

        double result = *n;
        for (it++; it != c.positional().end(); it++) {
            auto n = it->as<double>();
            if (!n) {
                return undefined;
            }

            result = op(result, *n);
        }

        return result;
    };
}

auto builtin_floor(const CallContext &c) {
    auto n = c.get<double>(0);
    return n ? Value{std::floor(*n)} : undefined;
}

Value builtin_list(const CallContext &c) {
    return c.positional();
}


void register_builtins_values(Environment &env) {
    env.add_function("if", builtin_if);
    env.add_function("-", builtin_minus);
    env.add_function("+", builtin_bin_op([](auto a, auto b) { return a + b; }));
    env.add_function("*", builtin_bin_op([](auto a, auto b) { return a * b; }));
    env.add_function("/", builtin_bin_op([](auto a, auto b) { return a / b; }));
    env.add_function("%", builtin_bin_op([](auto a, auto b) { return static_cast<int>(a) % static_cast<int>(b); }));
    env.add_function("list", builtin_list);
    env.add_function("floor", builtin_floor);
}
