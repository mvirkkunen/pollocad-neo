#include <format>

#include "helpers.h"

namespace
{

Value builtin_if(const CallContext &c) {
    size_t size = c.positional().size();
    for (int i = 0; i < size; i++) {
        auto condOrElse = *c.get(i);

        if (i == size - 1) {
            if (auto pfunc = condOrElse.as<Function>()) {
                // else
                return (*pfunc)(c.empty());
            } else {
                return undefined;
            }
        }

        bool truthy;
        if (i == 0) {
            truthy = condOrElse.truthy();
        } else {
            if (auto pfunc = condOrElse.as<Function>()) {
                // condition
                truthy = (*pfunc)(c.empty()).truthy();
            } else {
                return undefined;
            }
        }

        i++;

        if (truthy) {
            if (i >= size) {
                return c.error("malformed if clause (no value after condition)");
            }

            if (auto pfunc = c.get(i)->as<Function>()) {
                // then
                return (*pfunc)(c.empty());
            } else {
                return undefined;
            }
        }
    }

    return undefined;
}

auto builtin_un_op(std::function<double(double)> op) {
    return [op](const CallContext &c) -> Value {
        if (c.positional().size() != 1) {
            return c.error("malformed unary operation (incorrect argument count)");
        }

        auto a = c.get(0);

        if (auto pna = a->as<double>()) {
            return op(*pna);
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
                    return c.error(std::format("Invalid swizzle access: .{}", *pstr));
                }

                result.push_back(index < plist->size() ? (*plist)[index] : undefined);
            }

            if (result.size() == 1) {
                return result[0];
            }

            return result;
        } else {
            return c.error(std::format("cannot index list with value of type {}", pindex->typeName()));
        }
    } else if (auto pstring = pval->as<std::string>()) {
        if (auto pnum = pindex->as<double>()) {
            size_t index = static_cast<size_t>(*pnum);
            return (index < pstring->size()) ? Value{std::string(1, (*pstring).at(index))} : undefined;
        } else {
            return c.error(std::format("cannot index string with value of type {}", pindex->typeName()));
        }
    } else {
        return c.error(std::format("Cannot index value of type {}", pval->typeName()));
    }
}

Value builtin_list(const CallContext &c) {
    return c.positional();
}

Value builtin_concat(const CallContext &c) {
    auto it = c.positional().cbegin();
    auto end = c.positional().cend();
    if (it == end) {
        return undefined;
    }

    if (it->as<ValueList>()) {
        ValueList result;

        for (; it != end; it++) {
            if (it->undefined()) {
                continue;
            } else if (auto plist = it->as<ValueList>()) {
                std::copy(plist->cbegin(), plist->cend(), std::back_inserter(result));
            } else {
                return c.error(std::format("concat arguments must all be of the same type or undefined (found list, then {})", it->typeName()));
            }
        }

        return result;
    } else if (it->as<std::string>()) {
        std::string result;

        for (; it != end; it++) {
            if (it->undefined()) {
                continue;
            } else if (auto pstr = it->as<std::string>()) {
                result += *pstr;
            } else {
                return c.error(std::format("concat arguments must all be of the same type or undefined (found string, then {})", it->typeName()));
            }
        }

        return result;
    } else {
        return c.error(std::format("cannot concat values of type {}", it->typeName()));
    }
}

Value builtin_type(const CallContext &c) {
    if (c.positional().empty()) {
        return undefined;
    }

    return c.positional().at(0).typeName();
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
