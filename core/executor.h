#pragma once

#include <optional>
#include <QAbstractTableModel>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QThreadPool>

#include "logmessage.h"
#include "executorresult.h"
#include "value.h"

struct ExecutionContext {
    QPromise<ExecutorResult> promise;
    std::vector<LogMessage> messages;
};

class CallContext {
public:
    CallContext(std::vector<Value> positional, std::unordered_map<std::string, Value> named, ExecutionContext &execContext, const Span &span) :
        m_positional(positional), m_named(named), m_execContext(execContext), m_span(span) { }

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

    bool canceled() const { return m_execContext.promise.isCanceled(); }

    RuntimeError error(const std::string &msg) const {
        m_execContext.messages.push_back(LogMessage{LogMessage::Level::Error, msg, m_span});
        return RuntimeError{msg};
    }

    void warning(const std::string &msg) const {
        m_execContext.messages.push_back(LogMessage{LogMessage::Level::Warning, msg, m_span});
    }

    void info(const std::string &msg) const {
        m_execContext.messages.push_back(LogMessage{LogMessage::Level::Info, msg, m_span});
    }

    const TaggedShapes children() const;

    CallContext empty() const {
        return CallContext{{}, {}, m_execContext, m_span};
    }

    CallContext with(const std::string &name, Value value) const {
        return CallContext{{}, {{name, std::move(value)}}, m_execContext, m_span};
    }

private:
    std::vector<Value> m_positional;
    std::unordered_map<std::string, Value> m_named;
    ExecutionContext& m_execContext;
    const Span &m_span;
};

struct Environment {
    std::shared_ptr<Environment> parent = nullptr;
    std::unordered_map<std::string, Value> vars;

    bool get(const std::string &name, Value &out) const;
    void add_function(const std::string &name, const FunctionImpl& func) { vars.emplace(name, std::make_shared<FunctionImpl>(func)); }
};

class Executor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged);

    class Worker;

public:
    Executor();
    Q_INVOKABLE void execute(QString code);
    Value executeSync(QString code);
    bool isBusy() const;

signals:
    void isBusyChanged();
    void result(ExecutorResult *result);
    void logMessage(LogMessage msg);

private:
    QThreadPool m_threadPool;
    QFutureWatcher<ExecutorResult> m_futureWatcher;
};
