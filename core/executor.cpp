#include <memory>
#include <variant>
#include <QThread>
#include <QDebug>

#include "executor.h"
#include "parser.h"

#include <TopoDS_Shape.hxx>
#include <BRep_Builder.hxx>

extern void register_builtins_values(Environment &env);
extern void register_builtins_shapes(Environment &env);

namespace
{
Value eval(std::shared_ptr<QPromise<Result>> promise, const std::shared_ptr<Environment> &env, const ast::Expr* expr);
Value eval(std::shared_ptr<QPromise<Result>> promise, const std::shared_ptr<Environment> &env, const ast::ExprList* exprs);
Value eval(std::shared_ptr<QPromise<Result>> promise, const ast::ExprList* exprs);
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

Value Environment::get(const std::string &name) const {
    auto it = vars.find(name);
    if (it != vars.end()) {
        return it->second;
    }

    if (parent) {
        return parent->get(name);
    }

    return undefined;
}

class ExecutorThread : public QRunnable {
public:
    ExecutorThread(QPromise<Result> promise, const std::string &code)
        : m_code(code), m_promise(std::make_shared<QPromise<Result>>(std::move(promise)))
    {}

    void run() override;

public:
    std::string m_code;
    std::shared_ptr<QPromise<Result>> m_promise;
};

Executor::Executor() {
    m_threadPool.setMaxThreadCount(2);

    connect(&m_futureWatcher, &QFutureWatcher<Result>::finished, this, [this]() {
        auto r = m_futureWatcher.result();
        emit result(&r);
    });
}

void Executor::execute(QString code) {
    QPromise<Result> promise;
    auto future = promise.future();
    m_futureWatcher.cancel();
    m_futureWatcher.setFuture(future);

    m_threadPool.start(new ExecutorThread(std::move(promise), code.toStdString()));
}

Value Executor::executeSync(QString code) {
    auto root = parse(code.toStdString());
    if (!root) {
        return RuntimeError{"lol is actually parse error"};
    }

    return eval(std::make_shared<QPromise<Result>>(), &*root);
}

void ExecutorThread::run() {
    QElapsedTimer timer;
    timer.start();

    std::cerr << "Starting evaluation\n";

    auto root = parse(m_code);
    if (!root) {
        m_promise->addResult(Result("Parse error", {}));
        return;
    }

    auto result = eval(m_promise, &*root);

    const TaggedShapes *shapes = result.as<TaggedShapes>();
    Shape *shape = nullptr;
    if (shapes) {
        TopoDS_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);

        for (const auto &sh : *shapes) {
            builder.Add(comp, sh.shape);
        }

        shape = new Shape(comp);
    }

    auto elapsed = timer.elapsed();
    std::cerr << "Evaluation took " << elapsed << "ms, result: " << result << "\n";

    m_promise->addResult(Result{"", shape});
}

namespace
{

Value eval(std::shared_ptr<QPromise<Result>> promise, const std::shared_ptr<Environment> &env, const ast::Expr* expr) {
    if (promise->isCanceled()) {
        return undefined;
    }

    return std::visit<Value>(
        [env, &promise](const auto &ex) -> Value {
            using T = std::decay_t<decltype(ex)>;
            if constexpr (std::is_same_v<T, ast::CallExpr>) {
                auto func = env->get(ex.func).template as<Function>();
                if (!func) {
                    std::cerr << "Function not found: " << ex.func << "\n";
                    return undefined;
                }

                auto new_env = std::make_shared<Environment>();
                new_env->parent = env;

                std::vector<Value> positional;
                for (const auto &ch : ex.positional) {
                    if (promise->isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(promise, env, &ch);
                    if (val.error()) {
                        return val;
                    }

                    positional.push_back(val);
                }

                std::unordered_map<std::string, Value> named;
                for (const auto &[name, ch] : ex.named) {
                    if (promise->isCanceled()) {
                        return undefined;
                    }

                    auto val = eval(promise, env, &*ch);
                    if (val.error()) {
                        return val;
                    }

                    named.insert({name, val});
                }

                return (**func)(CallContext{positional, named, promise});
            } else if constexpr (std::is_same_v<T, ast::NumberExpr>) {
                return ex.value;
            } else if constexpr (std::is_same_v<T, ast::StringExpr>) {
                return ex.value;
            } else if constexpr (std::is_same_v<T, ast::VarExpr>) {
                return env->get(ex.name);
            } else if constexpr (std::is_same_v<T, ast::AssignExpr>) {
                auto val = eval(promise, env, &*ex.value);
                if (val.error()) {
                    return val;
                }

                auto new_env = std::make_shared<Environment>();
                new_env->parent = env;
                new_env->vars.insert({ex.name, val});
                return eval(promise, new_env, &ex.exprs);
            } else if constexpr (std::is_same_v<T, ast::BlockExpr>) {
                return std::make_shared<FunctionImpl>([promise, env, exprs=&ex.exprs](const CallContext &c) {
                    auto new_env = std::make_shared<Environment>();
                    new_env->parent = env;

                    for (const auto &[name, value] : c.named()) {
                        new_env->vars.emplace(name, value);
                    }

                    return eval(promise, new_env, exprs);
                });
            } else if constexpr (std::is_same_v<T, ast::ReturnExpr>) {
                return eval(promise, env, &*ex.expr);
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        expr->inner());
}

Value eval(std::shared_ptr<QPromise<Result>> promise, const std::shared_ptr<Environment> &env, const ast::ExprList* exprs) {
    Value result = undefined;
    TaggedShapes shapes;

    for (const auto& expr : *exprs) {
        if (promise->isCanceled()) {
            return undefined;
        }

        auto val = eval(promise, env, &expr);
        if (val.error()) {
            return val;
        }

        if (auto resultShapes = val.as<TaggedShapes>()) {
            std::move(resultShapes->begin(), resultShapes->end(), std::back_inserter(shapes));
        } else /*if (std::holds_alternative<ast::ReturnExpr>(expr.inner()))*/ {
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

Value eval(std::shared_ptr<QPromise<Result>> promise, const ast::ExprList* exprs) {
    auto env = std::make_shared<Environment>();
    register_builtins_values(*env);
    register_builtins_shapes(*env);

    return eval(promise, env, &*exprs);
}

}
