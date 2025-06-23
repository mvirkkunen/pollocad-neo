#pragma once

#include <atomic>
#include <format>
#include <mutex>
#include <vector>

#include "value.h"
#include "logmessage.h"

class ExecutionContext {
public:
    ExecutionContext(const std::shared_ptr<std::atomic_bool> canceled) : m_canceled(canceled) { }

    template <typename... Args>
    void addMessage(LogMessage::Level level, Span span, std::format_string<Args...> fmt, Args&&... args) {
        const auto msg = std::format(fmt, std::forward<Args>(args)...);
        std::unique_lock lock(m_messagesLock);
        m_messages.push_back({level, msg, span});
    }

    bool isCanceled() { return m_canceled->load(); }
    const std::vector<LogMessage> &messages() { return m_messages; }

private:
    std::shared_ptr<std::atomic_bool> m_canceled;
    std::mutex m_messagesLock;
    std::vector<LogMessage> m_messages;
};

class CallContext {
public:
    CallContext(ExecutionContext &execContext, std::vector<Value> positional, std::unordered_map<std::string, Value> named, const Span &span) :
        m_execContext(execContext), m_positional(positional), m_named(named), m_span(span) { }

    ExecutionContext &execContext() const { return m_execContext; }

    bool canceled() const { return m_execContext.isCanceled(); }

    const std::vector<Value> &positional() const { return m_positional; }

    const std::unordered_map<std::string, Value> &named() const { return m_named; }

    const Value *get(size_t index) const {
        return index < m_positional.size() ? &m_positional.at(index) : nullptr;
    }

    const Value *get(const std::string &name) const {
        auto it = m_named.find(name);
        return it != m_named.end() ? &it->second : nullptr;
    }

    template <typename T>
    const OptionalValue<T> get(size_t index, bool typeError=true) const {
        auto v = get(index);
        if (!v) {
            return {};
        }

        auto tv = v->as<T>();
        if (!tv && typeError) {
            warning("parameter {}: expected {}, got {}", index + 1, Value::typeName(Value::typeOf<T>()), v->typeName());
        }

        return tv;
    }

    template <typename T>
    const OptionalValue<T> get(const std::string &name, bool typeError=true) const {
        auto v = get(name);
        if (!v) {
            return {};
        }

        auto tv = v->as<T>();
        if (!tv && typeError) {
            warning("parameter {}: expected {}, got {}", name, Value::typeName(Value::typeOf<T>()), v->typeName());
        }

        return tv;
    }

    const ShapeList children() const;

    template <typename... Args>
    Value error(std::format_string<Args...> fmt, Args&&... args) const {
        m_execContext.addMessage(LogMessage::Level::Error, m_span, fmt, std::forward<Args>(args)...);
        return undefined;
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) const {
        m_execContext.addMessage(LogMessage::Level::Warning, m_span, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) const {
        m_execContext.addMessage(LogMessage::Level::Info, m_span, fmt, std::forward<Args>(args)...);
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

    const Span& span() const { return m_span; }

private:
    ExecutionContext& m_execContext;
    const std::vector<Value> m_positional;
    const std::unordered_map<std::string, Value> m_named;
    const Span &m_span;
};

class Environment {
public:
    explicit Environment(std::shared_ptr<Environment> parent) : m_parent(parent) { }

    bool isDefined(const std::string &name) const;
    bool set(const std::string &name, Value value);
    bool setFunction(const std::string &name, Function func);
    bool get(const std::string &name, Value &out) const;

private:
    std::shared_ptr<Environment> m_parent = nullptr;
    std::unordered_map<std::string, Value> m_vars;
};