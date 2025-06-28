#include <format>
#include <memory>
#include <variant>

#include "executor.h"
#include "contexts.h"
#include "parser.h"

#include <TopoDS_Shape.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>

extern void register_builtins_values(Environment &env);
extern void register_builtins_shapes(Environment &env);

namespace
{

Value eval(ExecutionContext &context, std::shared_ptr<Environment> env, const ast::Expr* expr);

}

#define REGISTER_BUILTINS(NAME) { extern void add_builtins_##NAME(Environment &env); add_builtins_##NAME(*m_defaultEnvironment); }

Executor::Executor() {
    m_defaultEnvironment = std::make_shared<Environment>(nullptr);

    REGISTER_BUILTINS(chamfer_fillet);
    REGISTER_BUILTINS(make_2d);
    REGISTER_BUILTINS(make_3d);
    REGISTER_BUILTINS(primitives);
    REGISTER_BUILTINS(shape_manipulation);
}

ExecutorResult Executor::execute(const std::string &code) {
    auto cancel = std::make_shared<std::atomic_bool>();

    auto cancelPrevious = m_cancelCurrent.exchange(cancel);
    if (cancelPrevious) {
        cancelPrevious->store(true);
    }

    ExecutionContext context{cancel};

    const auto parserResult = parse(code);

    std::vector<LogMessage> messages;

    std::copy(parserResult.errors.cbegin(), parserResult.errors.cend(), std::back_inserter(messages));
    if (!parserResult.result) {
        cancel->store(true);
        return ExecutorResult{std::nullopt, parserResult.errors};
    }

    auto env = std::make_shared<Environment>(m_defaultEnvironment);

    std::optional<Value> result = eval(context, env, &*parserResult.result);
    if (context.isCanceled()) {
        result = std::nullopt;
    }

    std::copy(context.messages().cbegin(), context.messages().cend(), std::back_inserter(messages));

    cancel->store(true);
    return ExecutorResult{result, messages};
}

bool Executor::isBusy() const {
    auto cancelCurrent = m_cancelCurrent.load();
    return cancelCurrent && !cancelCurrent->load();
}

namespace
{

void addHighlighted(ShapeList &shapes, const Value &value) {
    if (auto pshape = value.as<ShapeList>()) {
        std::copy_if(
            pshape->begin(),
            pshape->end(),
            std::back_inserter(shapes),
            [](const auto &sh) { return sh.hasProp("highlight"); }
        );
    }
}

struct UserFunction {
    std::weak_ptr<Environment> parentEnv;
    ast::LambdaExpr expr;
    std::unordered_map<std::string, Value> defaults;

    Value operator()(const CallContext &c) {
        auto parentEnvPtr = parentEnv.lock();
        if (!parentEnvPtr) {
            c.warning("attempted to call an escaped function - this is not supported");
            return undefined;
        }

        auto env = std::shared_ptr<Environment>(new Environment(parentEnvPtr));

        size_t i = 0;
        for (const auto& val : c.positional()) {
            if (i > expr.args.size()) {
                return c.error("too many arguments for function {}", expr.name);
            }

            env->set(expr.args[i].name, val);
            i++;
        }

        for (const auto& [name, val] : c.named()) {
            if (!name.starts_with("$")) {
                if (defaults.find(name) == defaults.end()) {
                    return c.error("function {} does not take argument {}", expr.name, name);
                }
            }

            env->set(name, val);
        }

        for (const auto &[name, value] : defaults) {
            if (!env->isDefined(name)) {
                env->set(name, value);
            }
        }

        return eval(c.execContext(), env, &*expr.body);
    }
};

Value eval(ExecutionContext &context, std::shared_ptr<Environment> env, const ast::Expr* expr) {
    if (context.isCanceled()) {
        return undefined;
    }

    return std::visit<Value>(
        [env, &context](const auto &ex) -> Value {
            using T = std::decay_t<decltype(ex)>;
            if constexpr (std::is_same_v<T, ast::BlockExpr>) {
                Value result = undefined;
                ShapeList shapes;

                for (const auto& expr : ex.exprs) {
                    if (context.isCanceled()) {
                        return undefined;
                    }

                    Value val = eval(context, env, &expr);
                    if (auto resultShapes = val.as<ShapeList>()) {
                        std::move(resultShapes->begin(), resultShapes->end(), std::back_inserter(shapes));
                    } else {
                        if (!shapes.empty() && !val.undefined()) {
                            context.addMessage(LogMessage::Level::Error, ex.span, "cannot return both shapes and a value");
                            return undefined;
                        }

                        result = val;
                    }
                }

                if (shapes.empty()) {
                    return result;
                }

                return shapes;
            } else if constexpr (std::is_same_v<T, ast::LiteralExpr>) {
                return ex.value;
            } else if constexpr (std::is_same_v<T, ast::VarExpr>) {
                Value val;
                if (!env->get(ex.name, val)) {
                    context.addMessage(LogMessage::Level::Warning, ex.span, "name '{}' not found", ex.name);
                    return undefined;
                }

                return val;
            } else if constexpr (std::is_same_v<T, ast::LetExpr>) {
                auto val = eval(context, env, &*ex.value);

                if (!env->set(ex.name, val)) {
                    context.addMessage(LogMessage::Level::Error, ex.span, "'{}' is already defined", ex.name);
                }

                return ex.return_ ? val : undefined;
            } else if constexpr (std::is_same_v<T, ast::CallExpr>) {
                bool error = false;

                Value funcVal;
                if (!env->get(ex.func, funcVal)) {
                    context.addMessage(LogMessage::Level::Warning, ex.span, "function '{}' not found", ex.func);
                    error = true;
                }

                auto func = funcVal.as<Function>();
                if (!func && !error) {
                    context.addMessage(LogMessage::Level::Warning, ex.span, "'{}' is of type '{}', not a function", ex.func, funcVal.typeName());
                    error = true;
                }

                ShapeList highlighted;

                std::vector<Value> positional;
                for (const auto &ch : ex.positional) {
                    if (context.isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &ch);
                    addHighlighted(highlighted, val);
                    positional.push_back(val);
                }

                std::unordered_map<std::string, Value> named;
                for (const auto &[name, ch] : ex.named) {
                    if (context.isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &*ch);
                    addHighlighted(highlighted, val);
                    named.emplace(name, val);
                }

                if (error) {
                    return undefined;
                }

                Value result = undefined;
                try {
                    result = (*func)(CallContext(context, positional, named, ex.span));
                } catch (Standard_Failure &exc) {
                    context.addMessage(LogMessage::Level::Warning, ex.span, "exception in built-in function: {}: {}", typeid(exc).name(), exc.GetMessageString());
                } catch (...) {
                    context.addMessage(LogMessage::Level::Error, ex.span, "unknown exception during processing");
                }

                if (!highlighted.empty()) {
                    if (result.undefined()) {
                        return highlighted;
                    } else if (auto pshapes = result.as<ShapeList>()) {
                        ShapeList combined = *pshapes;
                        std::copy(highlighted.begin(), highlighted.end(), std::back_inserter(combined));
                        return combined;
                    } else {
                        //context->messages().push_back(LogMessage{LogMessage::Level::Warning, "could not show highlighted argument because function did not return shapes"});
                    }
                }

                return result;
            } else if constexpr (std::is_same_v<T, ast::LambdaExpr>) {
                std::unordered_map<std::string, Value> defaults;
                for (const auto& arg : ex.args) {
                    if (context.isCanceled()) {
                        return undefined;
                    }

                    if (!arg.default_) {
                        continue;
                    }

                    auto val = eval(context, env, &**arg.default_);
                    defaults.emplace(arg.name, val);
                }

                return std::function<Value(const CallContext &)>{UserFunction{env, ex, defaults}};
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        expr->cinner());
}

}
