#include <format>

#include "helpers.h"

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
        bool truthy = (i == 0) ? condOrElse : condOrElse.as<Function>()(ec).truthy();

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
        auto arg = c.get("argument");

        if (arg->is<double>()) {
            return op(arg->as<double>());
        } else if (auto pla = a->as<ValueList>()) {
            ValueList result;
            result.reserve(pla->size());

            for (int i = 0; i < pla->size(); i++) {
                if ((*pla)[i].undefined()) {
                    result.push_back(undefined);
                } else {
                    auto na = (*pla)[i].as<double>();
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

        if (auto pna = a->as<double>()) {
            if (auto pnb = b->as<double>()) {
                return op(*pna, *pnb);
            } else {
                return c.error("both operands must be either numbers or lists");
            }
        } else if (auto pla = a->as<ValueList>()) {
            if (auto plb = b->as<ValueList>()) {
                ValueList result;
                result.reserve(pla->size());

                for (int i = 0; i < pla->size(); i++) {
                    if ((*pla)[i].undefined() || i >= plb->size() || (*plb)[i].undefined()) {
                        result.push_back(undefined);
                    } else {
                        auto pna = (*pla)[i].as<double>();
                        if (!pna) {
                            return c.error("list items must be either numbers or undefined");
                        }

                        auto pnb = (*plb)[i].as<double>();
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

constexpr auto builtin_equal(bool equal) {
    return [equal](const CallContext &c) {
        bool result = equal;
        for (size_t i = 0; i < c.positional().size() - 1; i++) {
            if ((*c.get(i) == *c.get(i + 1)) != equal) {
                return false;
            }
        }

        return true;
    };
}

Value builtin_logical_not(const CallContext &c) {
    auto pval = c.get(0);
    return pval && pval->truthy();
}

Value builtin_logical_and(const CallContext &c) {
    auto pcond = c.get(0);
    if (!pcond) {
        return undefined;
    }

    if (!pcond->truthy()) {
        return *pcond;
    }

    auto presult = c.get(1);
    return presult ? *presult : undefined;
}

Value builtin_logical_or(const CallContext &c) {
    auto pcond = c.get(0);
    if (pcond && pcond->truthy()) {
        return *pcond;
    }

    auto presult = c.get(1);
    return presult ? *presult : undefined;
}


Value builtin_index(const CallContext &c) {
    auto pval = c.get(0);
    if (!pval) {
        return c.error("malformed indexing operation (no value to index)");
    }

    auto pindex = c.get(1);
    if (!pindex) {
        return c.error("malformed indexing operation (missing index value)");
    }

    if (auto plist = pval->as<ValueList>()) {
        if (auto pnum = pindex->as<double>()) {
            size_t index = static_cast<size_t>(*pnum);
            return (index < plist->size()) ? (*plist)[index] : undefined;
        } else if (auto pstr = pindex->as<std::string>()) {
            std::vector<Value> result;
            for (const char ch : *pstr) {
                ssize_t index = -1;
                switch (ch) {
                    case 'x': case 'r': index = 0; break;
                    case 'y': case 'g': index = 1; break;
                    case 'z': case 'b': index = 2; break;
                    case 'w': case 'a': index = 3; break;
                }

                if (index == -1) {
                    return c.error("Invalid swizzle access: .{}", *pstr);
                }

                result.push_back(index < plist->size() ? (*plist)[index] : undefined);
            }

            if (result.size() == 1) {
                return result[0];
            }

            return result;
        } else {
            return c.error("cannot index list with value of type {}", pindex->typeName());
        }
    } else if (auto pstring = pval->as<std::string>()) {
        if (auto pnum = pindex->as<double>()) {
            size_t index = static_cast<size_t>(*pnum);
            return (index < pstring->size()) ? Value{std::string(1, (*pstring).at(index))} : undefined;
        } else {
            return c.error("cannot index string with value of type {}", pindex->typeName());
        }
    } else {
        return c.error("Cannot index value of type {}", pval->typeName());
    }
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

    if (it->as<ValueList>()) {
        ValueList result;

        for (; it != end; it++) {
            if (!*it) {
                continue;
            } else if (auto plist = it->as<ValueList>()) {
                std::copy(plist->cbegin(), plist->cend(), std::back_inserter(result));
            } else {
                return c.error("concat arguments must all be of the same type or undefined (found list, then {})", it->typeName());
            }
        }

        return result;
    } else if (it->as<std::string>()) {
        std::string result;

        for (; it != end; it++) {
            if (!*it) {
                continue;
            } else if (auto pstr = it->as<std::string>()) {
                result += *pstr;
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
