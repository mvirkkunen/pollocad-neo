#include <format>

#include "contexts.h"

namespace
{

Value builtin_if(const CallContext &c) {
    if (c.positional().size() < 2) {
        return c.error("Malformed if clause (too few arguments)");
    }

    if (c.get(0).truthy()) {
        auto then = c.get<Function>(1);
        if (!then) {
            return c.error("Invalid then block in if clause");
        }

        return (**then)(c.empty());
    }

    if (c.canceled()) {
        return undefined;
    }

    if (c.positional().size() == 2) {
        return undefined;
    }

    if (c.positional().size() == 3) {
        auto else_ = c.get<Function>(2);
        if (!else_) {
            return c.error("Invalid else block in if clause");
        }

        return (**else_)(c.empty());
    }

    return c.error("Malformed if clause (too many arguments)");
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

Value builtin_floor(const CallContext &c) {
    auto n = c.get<double>(0);
    return n ? Value{std::floor(*n)} : undefined;
}

Value builtin_index(const CallContext &c) {
    auto alist = c.get<List>(0);
    if (alist) {
        auto aindex = c.get<double>(1);
        if (aindex) {
            size_t index = static_cast<size_t>(*aindex);
            return (index < alist->size()) ? (*alist)[index] : undefined;
        }

        auto aname = c.get<std::string>(1);
        if (aname) {
            std::vector<Value> result;
            for (const char ch : *aname) {
                ssize_t index = -1;
                switch (ch) {
                    case 'x': case 'r': index = 0; break;
                    case 'y': case 'g': index = 1; break;
                    case 'z': case 'b': index = 2; break;
                    case 'w': case 'a': index = 3; break;
                }

                if (index == -1) {
                    return c.error(std::format("Invalid swizzle access: .{}", *aname));
                }

                result.push_back(index < alist->size() ? (*alist)[index] : undefined);
            }

            if (result.size() == 1) {
                return result[0];
            }

            return result;
        }

        return c.error("Invalid type of index for indexing a list");
    }

    auto astring = c.get<std::string>(0);
    if (astring) {
        auto aindex = c.get<double>(1);
        if (aindex) {
            size_t index = static_cast<size_t>(*aindex);
            return (index < astring->size()) ? Value{std::string(1, (*astring).at(index))} : undefined;
        }

        return c.error("Invalid type of index for indexing a string");
    }

    return c.error("Cannot index value of this type");
}

Value builtin_list(const CallContext &c) {
    return c.positional();
}

Value builtin_type(const CallContext &c) {
    if (c.positional().empty()) {
        return undefined;
    }

    return c.positional().at(0).type();
}

Value builtin_str(const CallContext &c) {
    if (c.positional().empty()) {
        return undefined;
    }

    std::stringstream ss;
    c.positional().at(0).display(ss);
    return ss.str();
}

Value builtin_echo(const CallContext &c) {
    std::stringstream ss;

    for (const auto &arg : c.positional()) {
        arg.display(ss);
    }

    for (const auto &arg: c.named()) {
        ss << arg.first << "=";
        arg.second.display(ss);
    }

    c.info(ss.str());
    return undefined;
}

}

void register_builtins_values(Environment &env) {
    env.setFunction("[]", builtin_index);
    env.setFunction("if", builtin_if);
    env.setFunction("-", builtin_minus);
    env.setFunction("+", builtin_bin_op([](auto a, auto b) { return a + b; }));
    env.setFunction("*", builtin_bin_op([](auto a, auto b) { return a * b; }));
    env.setFunction("/", builtin_bin_op([](auto a, auto b) { return a / b; }));
    env.setFunction("%", builtin_bin_op([](auto a, auto b) { return static_cast<int>(a) % static_cast<int>(b); }));
    env.setFunction("list", builtin_list);
    env.setFunction("floor", builtin_floor);
    env.setFunction("type", builtin_type);
    env.setFunction("str", builtin_str);
    env.setFunction("echo", builtin_echo);
}
