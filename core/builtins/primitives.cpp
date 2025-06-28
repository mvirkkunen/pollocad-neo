#include <format>

#include "contexts.h"
//#include "helpers.h"

namespace
{

Value builtin_if(const CallContext &c) {
    auto args = c.allPositional();
    for (int i = 0; i < args.size(); i++) {
        auto condOrElse = args.at(i);

        if (i == args.size() - 1) {
            auto ec = c.empty();
            return condOrElse.as<Function>()(ec);
        }

        auto ec = c.empty();
        bool truthy = ((i == 0) ? condOrElse : condOrElse.as<Function>()(ec)).isTruthy();

        i++;

        if (truthy) {
            if (i >= args.size()) {
                return c.error("malformed if clause (no value after condition)");
            }

            auto ec = c.empty();
            return args.at(i).as<Function>()(ec);
        }
    }

    return undefined;
}

auto builtin_un_op(std::function<double(double)> op) {
    return [op](CallContext &c) -> Value {
        return c.arg("operand").overload(
            [op](double value) -> Value { return op(value); },
            [op](const ValueList &vec) -> Value {
                ValueList result;
                result.reserve(vec.size());

                for (const auto &item : vec) {
                    if (item.is<double>()) {
                        result.push_back(op(item.as<double>()));
                    } else {
                        result.push_back(undefined);
                    }
                }

                return result;
            }
        );
    };
}

auto builtin_bin_op(std::function<double(double, double)> op) {
    return [op](CallContext &c) -> Value {
        return c.arg("left operand").overload(
            [&](double a) -> Value {
                auto b = c.arg("right operand 1").as<double>();
                return op(a, b);
            },
            [&](const ValueList &a) -> Value {
                auto ab = c.arg("right operand 2");
                auto b = ab.as<ValueList>();
                if (ab.asAny().is<ValueList>() && a.size() != b.size()) { // TODO ugly
                    return c.error("lists must be of equal size for binary operators");
                }

                ValueList result;
                result.reserve(a.size());

                for (int i = 0; i < a.size(); i++) {
                    auto ai = a.at(i);
                    auto bi = b.at(i);

                    if ((ai && !ai.is<double>()) && (bi && !bi.is<double>())) {
                        return c.error("list items must be either numbers or undefined");
                    } else if (!ai || !bi) {
                        result.push_back(undefined);
                    } else {
                        result.push_back(op(ai.as<double>(), bi.as<double>()));
                    }
                }

                return result;
            }
        );
    };
}

constexpr auto builtin_equal(bool equal) {
    return [equal](const CallContext &c) {
        bool result = equal;
        const auto &positional = c.allPositional();
        for (size_t i = 0; i < positional.size() - 1; i++) {
            if ((positional.at(i) == positional.at(i + 1)) != equal) {
                return false;
            }
        }

        return true;
    };
}

Value builtin_logical_not(CallContext &c) {
    return c.arg("operand").asAny().isTruthy();
}

Value builtin_logical_and(CallContext &c) {
    auto cond = c.arg("left operand").asAny();
    if (!cond.isTruthy()) {
        return cond;
    }

    return c.arg("right operand").asAny();
}

Value builtin_logical_or(CallContext &c) {
    auto cond = c.arg("left operand").asAny();
    if (cond.isTruthy()) {
        return cond;
    }

    return c.arg("right operand").asAny();
}


Value builtin_index(CallContext &c) {
    auto indexee = c.arg("indexee");
    auto index = c.arg("index");

    return indexee.overload(
        [&](const ValueList &list) -> Value {
            return index.overload(
                [&](double d) -> Value {
                    size_t i = static_cast<size_t>(d);
                    return (i < list.size()) ? list[i] : undefined;
                },
                [&](const std::string &str) -> Value {
                    std::vector<Value> result;
                    for (const char ch : str) {
                        ssize_t index = -1;
                        switch (ch) {
                            case 'x': case 'r': index = 0; break;
                            case 'y': case 'g': index = 1; break;
                            case 'z': case 'b': index = 2; break;
                            case 'w': case 'a': index = 3; break;
                        }

                        if (index == -1) {
                            return c.error("Invalid swizzle access: .{}", str);
                        }

                        result.push_back(index < list.size() ? list[index] : undefined);
                    }

                    if (result.size() == 1) {
                        return result[0];
                    }

                    return result;
                }
            );
        },
        [&](const std::string &str) -> Value {
            size_t i = static_cast<size_t>(index.as<double>());
            return (i < str.size()) ? Value{std::string(1, str.at(i))} : undefined;
        }
    );
}

Value builtin_list(CallContext &c) {
    return c.allPositional();
}

Value builtin_concat(const CallContext &c) {
    auto it = c.allPositional().cbegin();
    auto end = c.allPositional().cend();
    if (it == end) {
        return undefined;
    }

    // TODO: maybe make some kind of rest() function?

    if (it->is<ValueList>()) {
        ValueList result;

        for (; it != end; it++) {
            if (!*it) {
                continue;
            } else if (it->is<ValueList>()) {
                std::copy(it->as<ValueList>().cbegin(), it->as<ValueList>().cend(), std::back_inserter(result));
            } else {
                return c.error("concat arguments must all be of the same type or undefined (found list, then {})", it->typeName());
            }
        }

        return result;
    } else if (it->is<std::string>()) {
        std::string result;

        for (; it != end; it++) {
            if (!*it) {
                continue;
            } else if (it->is<std::string>()) {
                result += it->as<std::string>();
            } else {
                return c.error("concat arguments must all be of the same type or undefined (found string, then {})", it->typeName());
            }
        }

        return result;
    } else {
        return c.error("cannot concat values of type {}", it->typeName());
    }
}

Value builtin_type(CallContext &c) {
    return c.arg("value").asAny().typeName();
}

Value builtin_str(CallContext &c) {
    std::stringstream ss;
    c.arg("value").asAny().display(ss);
    return ss.str();
}

Value builtin_echo(CallContext &c) {
    std::stringstream ss;

    for (const auto &arg : c.allPositional()) {
        arg.display(ss);
    }

    for (const auto &arg: c.allNamed()) {
        ss << arg.first << "=";
        arg.second.display(ss);
    }

    c.info("{}", ss.str());
    return undefined;
}

}

void add_builtins_primitives(Environment &env) {
    env.setFunction("!", builtin_logical_not);
    env.setFunction("~", builtin_un_op([](auto a) { return ~static_cast<uint64_t>(a); }));

    env.setFunction("*", builtin_bin_op([](auto a, auto b) { return a * b; }));
    env.setFunction("/", builtin_bin_op([](auto a, auto b) { return a / b; }));
    env.setFunction("%", builtin_bin_op([](auto a, auto b) { return static_cast<int64_t>(a) % static_cast<int64_t>(b); }));
    env.setFunction("+", builtin_bin_op([](auto a, auto b) { return a + b; }));

    env.setFunction("-", builtin_bin_op([](auto a, auto b) { return a - b; }));

    env.setFunction("<", builtin_bin_op([](auto a, auto b) { return a < b; }));
    env.setFunction("<=", builtin_bin_op([](auto a, auto b) { return a <= b; }));
    env.setFunction(">", builtin_bin_op([](auto a, auto b) { return a > b; }));
    env.setFunction(">=", builtin_bin_op([](auto a, auto b) { return a >= b; }));

    env.setFunction("==", builtin_equal(true));
    env.setFunction("!=", builtin_equal(false));

    env.setFunction("&", builtin_bin_op([](auto a, auto b) { return static_cast<uint64_t>(a) & static_cast<uint64_t>(b); }));

    env.setFunction("|", builtin_bin_op([](auto a, auto b) { return static_cast<uint64_t>(a) & static_cast<uint64_t>(b); }));

    env.setFunction("&&", builtin_logical_and);

    env.setFunction("||", builtin_logical_or);

    env.setFunction("floor", builtin_un_op([](auto a) { return std::floor(a); }));
    env.setFunction("ceil", builtin_un_op([](auto a) { return std::ceil(a); }));
    env.setFunction("round", builtin_un_op([](auto a) { return std::round(a); }));

    env.setFunction("[]", builtin_index);

    env.setFunction("if", builtin_if);

    env.setFunction("list", builtin_list);
    env.setFunction("concat", builtin_concat);
    env.setFunction("str", builtin_str);
    env.setFunction("type", builtin_type);
    env.setFunction("echo", builtin_echo);
}
