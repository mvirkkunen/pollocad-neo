#pragma once

#include <optional>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>

#include "value.h"

class Shape : public QObject {
    Q_OBJECT

public:
    Shape() { }
    Shape(const Shape &shape) : m_shape(shape.m_shape) { }
    Shape(TopoDS_Shape shape) : m_shape(shape) { }

    TopoDS_Shape &shape() { return m_shape; }

private:
    TopoDS_Shape m_shape;
};

class Result : public QObject {
    Q_OBJECT

public:
    Result(const Result &other) : m_error(other.m_error), m_shape(other.m_shape ? new Shape(*other.m_shape) : nullptr) { }
    Result(std::string error, Shape *shape) : m_error(error), m_shape(shape) {
        if (m_shape) {
            m_shape->setParent(this);
        }
    }

public:
    Q_INVOKABLE QString error() { return QString::fromStdString(m_error); }
    Q_INVOKABLE Shape *shape() { return m_shape; }

private:
    std::string m_error;
    Shape *m_shape;
};

class CallContext {
public:
    CallContext(std::vector<Value> positional, std::unordered_map<std::string, Value> named, const std::shared_ptr<QPromise<Result>> &promise) :
        m_positional(positional), m_named(named), m_promise(promise) { }

    const std::vector<Value> &positional() const { return m_positional; }

    const std::unordered_map<std::string, Value> &named() const { return m_named; }

    size_t count() const { return m_positional.size(); }

    const Value &get(size_t index) const { return m_positional.at(index); }

    const Value &get(std::string name) const { return m_named.at(name); }

    template <typename T>
    const T *get(size_t index) const {
        return index < m_positional.size() ? m_positional.at(index).as<T>() : nullptr;
    }

    template <typename T>
    const T *get(std::string name) const {
        auto it = m_named.find(name);
        return it != m_named.end() ? it->second.as<T>() : nullptr;
    }

    bool canceled() const { return m_promise->isCanceled(); }

    const TaggedShapes children() const;

    CallContext empty() const {
        return CallContext{{}, {}, m_promise};
    }

    CallContext with(const std::string &name, Value value) const {
        return CallContext{{}, {{name, std::move(value)}}, m_promise};
    }

private:
    std::vector<Value> m_positional;
    std::unordered_map<std::string, Value> m_named;
    std::shared_ptr<QPromise<Result>> m_promise;
};

struct Environment {
    std::shared_ptr<Environment> parent;
    std::unordered_map<std::string, Value> vars;

    Value get(const std::string &name) const;
    void add_function(const std::string &name, const FunctionImpl& func) { vars.emplace(name, std::make_shared<FunctionImpl>(func)); }
};

class Executor : public QObject
{
    Q_OBJECT

    class Worker;

public:
    Executor();
    Q_INVOKABLE void execute(QString code);
    Value executeSync(QString code);

signals:
    void result(Result *result);

private:
    QThreadPool m_threadPool;
    QFutureWatcher<Result> m_futureWatcher;
    std::optional<QFuture<Result>> m_inProgress;
};
