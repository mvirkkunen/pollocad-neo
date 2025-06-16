#include <memory>
#include <variant>
#include <QThread>
#include <QDebug>

#include "executor.h"
#include "parser.h"

#include <TopoDS_Shape.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>

extern void register_builtins_values(Environment &env);
extern void register_builtins_shapes(Environment &env);

namespace
{

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::Expr* expr);
Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::ExprList* exprs);
Value eval(const std::shared_ptr<ExecutionContext> &context, const ast::ExprList* exprs);

}

const TaggedShapes CallContext::children() const {
    const auto block = get<Function>("$children");
    if (!block) {
        return {};
    }

    const auto value = (**block)(empty());
    const auto shapes = value.as<TaggedShapes>();
    if (!shapes) {
        return {};
    }

    return std::move(*shapes);
}

bool Environment::get(const std::string &name, Value &out) const {
    auto it = vars.find(name);
    if (it != vars.end()) {
        out = it->second;
        return true;
    }

    if (parent) {
        return parent->get(name, out);
    }

    out = undefined;
    return false;
}

class ExecutorThread : public QRunnable {
public:
    ExecutorThread(const std::shared_ptr<ExecutionContext> &context, const std::string &code) : m_context(context), m_code(code) {}

    void run() override;

public:
    std::shared_ptr<ExecutionContext> m_context;
    std::string m_code;
};

Executor::Executor() {
    m_threadPool.setMaxThreadCount(2);

    connect(&m_futureWatcher, &QFutureWatcher<ExecutorResult>::finished, this, [this]() {
        auto r = m_futureWatcher.result();
        emit isBusyChanged();
        emit result(&r);
    });
}

void Executor::execute(QString code) {
    QPromise<ExecutorResult> promise;
    auto future = promise.future();
    m_futureWatcher.cancel();
    m_futureWatcher.setFuture(future);
    emit isBusyChanged();

    auto context = std::make_shared<ExecutionContext>(std::move(promise));

    auto thread = new ExecutorThread(context, code.toStdString());

    m_threadPool.start(thread);
}

Value Executor::executeSync(QString code) {
    auto root = parse(code.toStdString());
    if (!root.result) {
        return RuntimeError{"Parse error in executeSync"};
    }

    auto context = std::make_shared<ExecutionContext>(QPromise<ExecutorResult>());

    return eval(context, &*root.result);
}

bool Executor::isBusy() const {
    return !m_futureWatcher.isFinished();
}

void ExecutorThread::run() {
    auto &promise = m_context->promise;

    promise.start();

    QElapsedTimer timer;
    timer.start();

    //m_logCallback(LogMessage::info("Starting evaluation"));

    auto parserResult = parse(m_code);
    //auto &errors = parserResult.errors;
    //auto errors = std::vector<LogMessage>(parserResult.errors.begin(), parserResult.errors.end());

    if (!parserResult.result) {
        promise.addResult(ExecutorResult{parserResult.errors, {}});
        promise.finish();
        return;
    }

    auto result = eval(m_context, &*parserResult.result);

    TopoDS_Compound shape;

    if (!result.undefined()) {
        const TaggedShapes *shapes = result.as<TaggedShapes>();

        if (shapes) {
            TopoDS_Builder builder;
            builder.MakeCompound(shape);
            for (const auto &sh : *shapes) {
                builder.Add(shape, sh.shape);
            }
        } else {
            m_context->messages.insert(m_context->messages.begin(), LogMessage{LogMessage::Level::Error, "Output value type was not a shape"});
        }
    }

    auto elapsed = timer.elapsed();

    //m_logCallback(LogMessage::info(std::format("Evaluation took {}ms", elapsed)));

    promise.addResult(ExecutorResult{m_context->messages, shape});
    promise.finish();
}

namespace
{

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::Expr* expr) {
    if (context->promise.isCanceled()) {
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
                    context->messages.push_back(LogMessage{LogMessage::Level::Warning, std::format("Name '{}' not found", ex.name), ex.span});
                    return undefined;
                }

                return val;
            } else if constexpr (std::is_same_v<T, ast::LetExpr>) {
                auto val = eval(context, env, &*ex.value);
                if (val.error()) {
                    return val;
                }

                if (env->vars.contains(ex.name)) {
                    context->messages.push_back(LogMessage{LogMessage::Level::Warning, std::format("'{}' is already defined", ex.name), ex.span});
                    return undefined;
                }

                env->vars.emplace(ex.name, val);
                return undefined;
            } else if constexpr (std::is_same_v<T, ast::CallExpr>) {
                Value funcVal;
                if (!env->get(ex.func, funcVal)) {
                    context->messages.push_back(LogMessage{LogMessage::Level::Warning, std::format("Function '{}' not found", ex.func), ex.span});
                    return undefined;
                }

                auto func = funcVal.as<Function>();
                if (!func) {
                    context->messages.push_back(LogMessage{LogMessage::Level::Warning, std::format("'{}' is not afunction", ex.func), ex.span});
                    return undefined;
                }

                std::vector<Value> positional;
                for (const auto &ch : ex.positional) {
                    if (context->promise.isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &ch);
                    if (val.error()) {
                        return val;
                    }

                    positional.push_back(val);
                }

                std::unordered_map<std::string, Value> named;
                for (const auto &[name, ch] : ex.named) {
                    if (context->promise.isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(context, env, &*ch);
                    if (val.error()) {
                        return val;
                    }

                    named.emplace(name, val);
                }

                return (**func)(CallContext{positional, named, *context, ex.span});
            } else if constexpr (std::is_same_v<T, ast::LambdaExpr>) {
                std::unordered_map<std::string, Value> defaults;
                for (const auto& arg : ex.args) {
                    if (context->promise.isCanceled()) {
                        return undefined;
                    }

                    if (!arg.default_) {
                        continue;
                    }

                    auto val = eval(context, env, &**arg.default_);
                    if (val.error()) {
                        return val;
                    }

                    defaults.emplace(arg.name, val);
                }

                std::shared_ptr<ExecutionContext> context2 = context;
                return std::make_shared<FunctionImpl>([context=context2 , env, ex, defaults](const CallContext &c) -> Value {
                    auto new_env = std::make_shared<Environment>();
                    new_env->parent = env;

                    auto &vars = new_env->vars;

                    size_t i = 0;
                    for (const auto& val : c.positional()) {
                        if (i > ex.args.size()) {
                            return RuntimeError{std::format("Too many arguments for function {}", ex.name)};
                        }

                        vars.emplace(ex.args[i].name, val);
                        i++;
                    }

                    for (const auto& [name, val] : c.named()) {
                        if (!name.starts_with("$")) {
                            if (defaults.find(name) == defaults.end()) {
                                return RuntimeError{std::format("Function {} does not take argument {}", ex.name, name)};
                            }
                        }

                        vars.emplace(name, val);
                    }

                    for (const auto &[name, value] : defaults) {
                        if (vars.find(name) == vars.end()) {
                            vars.emplace(name, value);
                        }
                    }

                    return eval(context, new_env, &ex.body);
                });
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        expr->inner());
}

Value eval(const std::shared_ptr<ExecutionContext> &context, std::shared_ptr<Environment> env, const ast::ExprList *exprs) {
    Value result = undefined;
    TaggedShapes shapes;

    for (const auto& expr : *exprs) {
        if (context->promise.isCanceled()) {
            return undefined;
        }

        auto val = eval(context, env, &expr);
        if (val.error()) {
            return val;
        }

        if (auto resultShapes = val.as<TaggedShapes>()) {
            std::move(resultShapes->begin(), resultShapes->end(), std::back_inserter(shapes));
        } else {
            if (!shapes.empty() && !val.undefined()) {
                return RuntimeError{"Cannot return both shapes and a value"};
            }

            result = val;
        }
    }

    if (shapes.empty()) {
        return result;
    }

    return shapes;
}

Value eval(const std::shared_ptr<ExecutionContext> &context, const ast::ExprList* exprs) {
    auto env = std::make_shared<Environment>();
    register_builtins_values(*env);
    register_builtins_shapes(*env);

    return eval(context, env, &*exprs);
}

}
