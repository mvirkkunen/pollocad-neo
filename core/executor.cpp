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

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::Expr* expr);

}

#define REGISTER_BUILTINS(NAME) { extern void add_builtins_##NAME(Environment &env); add_builtins_##NAME(*m_defaultEnvironment); }

Executor::Executor() {
    m_defaultEnvironment = std::make_shared<Environment>(nullptr);

    REGISTER_BUILTINS(chamfer_fillet);
    REGISTER_BUILTINS(primitive_shapes);
    REGISTER_BUILTINS(primitives);
    REGISTER_BUILTINS(shape_manipulation);
}

ExecutorResult Executor::execute(const std::string &code) {
    if (m_currentContext) {
        m_currentContext->cancel();
    }

    auto context = m_currentContext = std::make_shared<ExecutionContext>();

    const auto parserResult = parse(code);

    std::copy(parserResult.errors.cbegin(), parserResult.errors.cend(), std::back_inserter(context->messages()));
    if (!parserResult.result) {
        context->cancel();
        return ExecutorResult{std::nullopt, parserResult.errors};
    }

    auto env = std::make_shared<Environment>(m_defaultEnvironment);

    std::optional<Value> result = eval(context, env, &*parserResult.result);
    if (context->isCanceled()) {
        result = std::nullopt;
    }

    context->cancel();
    return ExecutorResult{result, context->messages()};
}

bool Executor::isBusy() const {
    return m_currentContext && !m_currentContext->isCanceled();
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
    std::shared_ptr<ExecutionContext> context;
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
                return c.error(std::format("Too many arguments for function {}", expr.name));
            }

            env->set(expr.args[i].name, val);
            i++;
        }

        for (const auto& [name, val] : c.named()) {
            if (!name.starts_with("$")) {
                if (defaults.find(name) == defaults.end()) {
                    return c.error(std::format("Function {} does not take argument {}", expr.name, name));
                }
            }

            env->set(name, val);
        }

        for (const auto &[name, value] : defaults) {
            if (!env->isDefined(name)) {
                env->set(name, value);
            }
        }

        return eval(context, env, &*expr.body);
    }
};

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::Expr* expr) {
    if (context->isCanceled()) {
        return undefined;
    }

    return std::visit<Value>(
        [env, context](const auto &ex) -> Value {
            using T = std::decay_t<decltype(ex)>;
            if constexpr (std::is_same_v<T, ast::BlockExpr>) {
                Value result = undefined;
                ShapeList shapes;

                for (const auto& expr : ex.exprs) {
                    if (context->isCanceled()) {
                        return undefined;
                    }

                    Value val = eval(context, env, &expr);
                    if (auto resultShapes = val.as<ShapeList>()) {
                        std::move(resultShapes->begin(), resultShapes->end(), std::back_inserter(shapes));
                    } else {
                        if (!shapes.empty() && !val.undefined()) {
                            context->messages().push_back(LogMessage{LogMessage::Level::Error, "Cannot return both shapes and a value", ex.span});
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
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, std::format("Name '{}' not found", ex.name), ex.span});
                    return undefined;
                }

                return val;
            } else if constexpr (std::is_same_v<T, ast::LetExpr>) {
                auto val = eval(context, env, &*ex.value);

                if (!env->set(ex.name, val)) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Error, std::format("'{}' is already defined", ex.name), ex.span});
                }

                return ex.return_ ? val : undefined;
            } else if constexpr (std::is_same_v<T, ast::CallExpr>) {
                bool error = false;

                Value funcVal;
                if (!env->get(ex.func, funcVal)) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, std::format("Function '{}' not found", ex.func), ex.span});
                    error = true;
                }

                auto func = funcVal.as<Function>();
                if (!func && !error) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, std::format("'{}' is of type '{}', not a function", ex.func, funcVal.typeName()), ex.span});
                    error = true;
                }

                ShapeList highlighted;

                std::vector<Value> positional;
                for (const auto &ch : ex.positional) {
                    if (context->isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &ch);
                    addHighlighted(highlighted, val);
                    positional.push_back(val);
                }

                std::unordered_map<std::string, Value> named;
                for (const auto &[name, ch] : ex.named) {
                    if (context->isCanceled()) {
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
                    result = (*func)(CallContext(*context, positional, named, ex.span));
                } catch (Standard_Failure &exc) {
                    auto msg = std::format("Exception in built-in function: {}: {}", typeid(exc).name(), exc.GetMessageString());
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, msg, ex.span});
                } catch (...) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Error, "Unknown exception during processing", ex.span});
                }

                if (!highlighted.empty()) {
                    if (result.undefined()) {
                        return highlighted;
                    } else if (auto pshapes = result.as<ShapeList>()) {
                        ShapeList combined = *pshapes;
                        std::copy(highlighted.begin(), highlighted.end(), std::back_inserter(combined));
                        return combined;
                    } else {
                        //context->messages().push_back(LogMessage{LogMessage::Level::Warning, "Could not show highlighted argument because function did not return shapes"});
                    }
                }

                return result;
            } else if constexpr (std::is_same_v<T, ast::LambdaExpr>) {
                std::unordered_map<std::string, Value> defaults;
                for (const auto& arg : ex.args) {
                    if (context->isCanceled()) {
                        return undefined;
                    }

                    if (!arg.default_) {
                        continue;
                    }

                    auto val = eval(context, env, &**arg.default_);
                    defaults.emplace(arg.name, val);
                }

                return std::function<Value(const CallContext &)>{UserFunction{context, env, ex, defaults}};
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        expr->cinner());
}

}
