#include <format>

#include "helpers.h"

namespace
{

Value builtin_if(const CallContext &c) {
    auto pcond = c.get(0);
    if (!pcond) {
        return c.error("Malformed if clause (no condition)");
    }

    auto pthen = c.get<Function>(1);
    if (!pthen) {
        pthen = c.get<Function>("$children");
    }

    if (!pthen) {
        return c.error("Malformed if clause (no then expression)");
    }

    auto pelse = c.get<Function>(2);

    if (pcond->truthy()) {
        return (**pthen)(c.empty());
    } else if (pelse) {
        return (**pelse)(c.empty());
    } else {
        return undefined;
    }
}

auto builtin_un_op(std::function<double(double)> op) {
    return [op](const CallContext &c) -> Value {
        if (c.positional().size() != 1) {
            return c.error("malformed unary operation (incorrect argument count)");
        }

        auto a = c.get(0);

        if (auto *pna = a->as<double>()) {
            return op(*pna);
        } else if (auto *pla = a->as<List>()) {
            List result;
            result.reserve(pla->size());

            for (int i = 0; i < pla->size(); i++) {
                if ((*pla)[i].undefined()) {
                    result.push_back(undefined);
                } else {
                    auto *pna = (*pla)[i].as<double>();
                    if (!pna) {
                        return c.error("list items must be either numbers or undefined");
                    }

                    result.push_back(op(*pna));
                }
            }

            return result;
        } else {
            return c.error("operand must be either number or list");
        }
    };
}

auto builtin_bin_op(std::function<double(double, double)> op) {
    return [op](const CallContext &c) -> Value {
        if (c.positional().size() != 2) {
            return c.error("malformed binary operation (incorrect argument count)");
        }

        auto a = c.get(0);
        auto b = c.get(1);

        if (auto *pna = a->as<double>()) {
            if (auto *pnb = b->as<double>()) {
                return op(*pna, *pnb);
            } else {
                return c.error("both operands must be either numbers or lists");
            }
        } else if (auto *pla = a->as<List>()) {
            if (auto *plb = b->as<List>()) {
                List result;
                result.reserve(pla->size());

                for (int i = 0; i < pla->size(); i++) {
                    if ((*pla)[i].undefined() || i >= plb->size() || (*plb)[i].undefined()) {
                        result.push_back(undefined);
                    } else {
                        auto *pna = (*pla)[i].as<double>();
                        if (!pna) {
                            return c.error("list items must be either numbers or undefined");
                        }

                        auto *pnb = (*plb)[i].as<double>();
                        if (!pna) {
                            return c.error("list items must be either numbers or undefined");
                        }

                        result.push_back(op(*pna, *pnb));
                    }
                }

                return result;
            } else {
                return c.error("both operands must be either numbers or lists");
            }
        } else {
            return c.error("both operands must be either numbers or lists");
        }
    };
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

void add_builtins_primitives(Environment &env) {
    env.setFunction("!", builtin_un_op([](auto a) { return !static_cast<bool>(a); }));
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

    env.setFunction("==", builtin_bin_op([](auto a, auto b) { return a == b; }));
    env.setFunction("!=", builtin_bin_op([](auto a, auto b) { return a != b; }));

    env.setFunction("&", builtin_bin_op([](auto a, auto b) { return static_cast<uint64_t>(a) & static_cast<uint64_t>(b); }));

    env.setFunction("|", builtin_bin_op([](auto a, auto b) { return static_cast<uint64_t>(a) & static_cast<uint64_t>(b); }));

    env.setFunction("floor", builtin_un_op([](auto a) { return std::floor(a); }));
    env.setFunction("ceil", builtin_un_op([](auto a) { return std::ceil(a); }));
    env.setFunction("round", builtin_un_op([](auto a) { return std::round(a); }));

    env.setFunction("[]", builtin_index);

    env.setFunction("if", builtin_if);

    env.setFunction("list", builtin_list);
    env.setFunction("str", builtin_str);
    env.setFunction("type", builtin_type);
    env.setFunction("echo", builtin_echo);
}
