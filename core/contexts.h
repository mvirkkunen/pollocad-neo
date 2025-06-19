#include <atomic>
#include <vector>

#include "value.h"
#include "logmessage.h"

class ExecutionContext {
public:
    void cancel() { m_canceled.store(true); }
    bool isCanceled() { return m_canceled.load(); }
    std::vector<LogMessage> &messages() { return m_messages; }

private:
    std::atomic_bool m_canceled;
    std::vector<LogMessage> m_messages;
};

class CallContext {
public:
    CallContext(ExecutionContext &execContext, std::vector<Value> positional, std::unordered_map<std::string, Value> named, const Span &span) :
        m_execContext(execContext), m_positional(positional), m_named(named), m_span(span) { }

    bool canceled() const { return m_execContext.isCanceled(); }

    const std::vector<Value> &positional() const { return m_positional; }

    const std::unordered_map<std::string, Value> &named() const { return m_named; }

    //const Value &get(size_t index) const { return m_positional.at(index); }

    //const Value &get(std::string name) const { return m_named.at(name); }

    const Value *get(size_t index) const {
        return index < m_positional.size() ? &m_positional.at(index) : nullptr;
    }

    const Value *get(const std::string &name) const {
        auto it = m_named.find(name);
        return it != m_named.end() ? &it->second : nullptr;
    }

    template <typename T>
    const T *get(size_t index) const {
        auto v = get(index);
        return v ? v->as<T>() : nullptr;
    }

    template <typename T>
    const T *get(const std::string &name) const {
        auto v = get(name);
        return v ? v->as<T>() : nullptr;
    }

    const ShapeList children() const;

    Value error(const std::string &msg) const {
        m_execContext.messages().push_back(LogMessage{LogMessage::Level::Error, msg, m_span});
        return undefined;
    }

    void warning(const std::string &msg) const {
        m_execContext.messages().push_back(LogMessage{LogMessage::Level::Warning, msg, m_span});
    }

    void info(const std::string &msg) const {
        m_execContext.messages().push_back(LogMessage{LogMessage::Level::Info, msg, m_span});
    }

    CallContext empty() const {
        return CallContext{m_execContext, {}, {}, m_span};
    }

    CallContext with(Value value) const {
        return CallContext{m_execContext, {std::move(value)}, {}, m_span};
    }

    CallContext with(const std::string &name, Value value) const {
        return CallContext{m_execContext, {}, {{name, std::move(value)}}, m_span};
    }

private:
    ExecutionContext& m_execContext;
    std::vector<Value> m_positional;
    std::unordered_map<std::string, Value> m_named;
    const Span &m_span;
};

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent) : m_parent(parent) { }

    bool isDefined(const std::string &name) const;
    bool set(const std::string &name, Value value);
    bool setFunction(const std::string &name, FunctionImpl func);
    bool get(const std::string &name, Value &out) const;

private:
    std::shared_ptr<Environment> m_parent = nullptr;
    std::unordered_map<std::string, Value> m_vars;
};