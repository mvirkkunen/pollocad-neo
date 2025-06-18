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
Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::ExprList* exprs, const Span& span);

}

Executor::Executor() {
    m_defaultEnvironment = std::make_shared<Environment>(nullptr);

    extern void register_builtins_values(Environment &env);
    extern void register_builtins_shapes(Environment &env);
    register_builtins_values(*m_defaultEnvironment);
    register_builtins_shapes(*m_defaultEnvironment);
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

    auto span = Span{0, static_cast<int>(code.size()), 1, 1};

    std::optional<Value> result = eval(context, env, &*parserResult.result, span);
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

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::Expr* expr) {
    if (context->isCanceled()) {
        return undefined;
    }

    return std::visit<Value>(
        [env, context](const auto &ex) -> Value {
            using T = std::decay_t<decltype(ex)>;
            if constexpr (std::is_same_v<T, ast::LiteralExpr>) {
                return *ex.value;
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

                return undefined;
            } else if constexpr (std::is_same_v<T, ast::CallExpr>) {
                bool error = false;

                Value funcVal;
                if (!env->get(ex.func, funcVal)) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, std::format("Function '{}' not found", ex.func), ex.span});
                    error = true;
                }

                auto func = funcVal.as<Function>();
                if (!func && !error) {
                    context->messages().push_back(LogMessage{LogMessage::Level::Warning, std::format("'{}' is of type '{}', not a function", ex.func, funcVal.type()), ex.span});
                    error = true;
                }

                std::vector<Value> positional;
                for (const auto &ch : ex.positional) {
                    if (context->isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &ch);
                    positional.push_back(val);
                }

                std::unordered_map<std::string, Value> named;
                for (const auto &[name, ch] : ex.named) {
                    if (context->isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &*ch);
                    named.emplace(name, val);
                }

                if (error) {
                    return undefined;
                }

                return (**func)(CallContext(*context, positional, named, ex.span));
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

                std::shared_ptr<ExecutionContext> context2 = context;
                return std::make_shared<FunctionImpl>([context=context2, env, ex, defaults](const CallContext &c) -> Value {
                    auto newEnv = std::make_shared<Environment>(env);

                    size_t i = 0;
                    for (const auto& val : c.positional()) {
                        if (i > ex.args.size()) {
                            return c.error(std::format("Too many arguments for function {}", ex.name));
                        }

                        newEnv->set(ex.args[i].name, val);
                        i++;
                    }

                    for (const auto& [name, val] : c.named()) {
                        if (!name.starts_with("$")) {
                            if (defaults.find(name) == defaults.end()) {
                                return c.error(std::format("Function {} does not take argument {}", ex.name, name));
                            }
                        }

                        newEnv->set(name, val);
                    }

                    for (const auto &[name, value] : defaults) {
                        if (!newEnv->isDefined(name)) {
                            newEnv->set(name, value);
                        }
                    }

                    return eval(context, newEnv, &ex.body, ex.span);
                });
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        expr->inner());
}

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::ExprList *exprs, const Span &span) {
    Value result = undefined;
    ShapeList shapes;

    for (const auto& expr : *exprs) {
        if (context->isCanceled()) {
            return undefined;
        }

        auto val = eval(context, env, &expr);
        if (auto resultShapes = val.as<ShapeList>()) {
            std::move(resultShapes->begin(), resultShapes->end(), std::back_inserter(shapes));
        } else {
            if (!shapes.empty() && !val.undefined()) {
                context->messages().push_back(LogMessage{LogMessage::Level::Error, "Cannot return both shapes and a value", span});
                return undefined;
            }

            result = val;
        }
    }

    if (shapes.empty()) {
        return result;
    }

    return shapes;
}

}
