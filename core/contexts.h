#pragma once

#include <atomic>
#include <format>
#include <mutex>
#include <vector>

#include "value.h"
#include "logmessage.h"

class Argument;
class ArgumentList;

namespace
{

struct Void { };

template <typename F>
using OverloadArgType =
    std::conditional_t<std::is_invocable_v<F, Undefined>, Undefined,
    std::conditional_t<std::is_invocable_v<F, double>, double,
    std::conditional_t<std::is_invocable_v<F, std::string>, std::string,
    std::conditional_t<std::is_invocable_v<F, ValueList>, ValueList,
    std::conditional_t<std::is_invocable_v<F, ShapeList>, ShapeList,
    std::conditional_t<std::is_invocable_v<F, Function>, Function,
    std::conditional_t<std::is_invocable_v<F, Argument>, Argument,
    std::conditional_t<std::is_invocable_v<F, ArgumentList>, ArgumentList,
    std::conditional_t<std::is_invocable_v<F, bool>, bool, void>>>>>>>>>;

}

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

class Argument;

class CallContext {
public:
    CallContext(ExecutionContext &execContext, std::vector<Value> positional, std::unordered_map<std::string, Value> named, const Span &span) :
        m_execContext(execContext), m_positional(positional), m_named(named), m_span(span) { }

    ExecutionContext &execContext() const { return m_execContext; }

    bool canceled() const { return m_execContext.isCanceled(); }

    Argument arg(const char *name);
    Argument named(const char *name) const;
    //std::span<Value const> rest() const;

    const std::vector<Value> &allPositional() const { return m_positional; }

    const std::unordered_map<std::string, Value> &allNamed() const { return m_named; }

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
    size_t m_nextPositional = 0;
};

class ArgumentList;

class Argument {
    Argument(const CallContext &callContext, const char *name, const Value &value, bool isSubValue) : m_callContext(callContext), m_name(name), m_value(value), m_isSubValue(isSubValue) { }

public:
    Argument(const CallContext &callContext, const char *name, const Value &value) : Argument(callContext, name, value, false) { }

    const Argument subValue(const Value &value) const {
        return Argument(m_callContext, m_name, value, true);
    }

    /*const char *name() const {
        return m_name;
    }*/

    template <typename T>
    bool is() const {
        return m_value.is<T>();
    }

    template <typename T>
    ValueAs<T> as() const {
        if (m_value.isUndefined()) {
            m_callContext.error("missing required {}", descriptiveName());
        } else if (!m_value.is<T>()) {
            m_callContext.error("invalid {}: type is {}, expected {}", descriptiveName(), m_value.typeName(), Value::typeName(Value::typeOf<T>()));
        }

        return m_value.as<T>();
    }

    template <typename T>
    ValueAs<T> as(T default_) const {
        return m_value ? as<T>() : default_;
    }

    ArgumentList asList() const;

    const Value &asAny() const {
        return m_value;
    }

    bool isTruthy() const {
        return m_value.isTruthy();
    }

    template <typename Head, typename... Rest>
    auto overload(Head &&head, Rest&&... rest) const -> decltype(head({})) const {
        using ReturnType = decltype(head({}));

        std::conditional_t<std::is_same_v<ReturnType, void>, Void, ReturnType> ret;
        if (!overload_(ret, head, rest...)) {
            std::ostringstream ss;
            overloadTypeError(ss, head, rest...);
            m_callContext.error("invalid {}: type is {}, expected one of: {}", descriptiveName(), m_value.typeName(), ss.str());
        }

        if constexpr (!std::is_same_v<ReturnType, void>) {
            return ret;
        }
    }

    template <typename... Args>
    Value error(std::format_string<Args...> fmt, Args&&... args) const {
        return m_callContext.error("invalid {}: {}", descriptiveName(), std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args) const {
        m_callContext.warning("invalid {}: {}", descriptiveName(), std::format(fmt, std::forward<Args>(args)...));
    }

    operator bool() const { return !m_value.isUndefined(); }

private:
    const CallContext &m_callContext;
    const char *m_name;
    const Value &m_value;
    const bool m_isSubValue;

    std::string descriptiveName() const {
        return m_isSubValue ? std::format("item in {}", m_name) : std::format("argument {}", m_name);
    }

    template <typename Ret, typename Head, typename... Rest>
    bool overload_(Ret &ret, Head&& head, Rest&&... rest) const {
        using Type = OverloadArgType<Head>;

        if constexpr (std::is_same_v<Type, Argument>) {
            if constexpr (std::is_same_v<Ret, Void>) {
                head(*this);
            } else {
                ret = head(*this);
            }

            return true;
        } else if constexpr (std::is_same_v<Type, ArgumentList>) {
            if (m_value.is<ValueList>()) {
                if constexpr (std::is_same_v<Ret, Void>) {
                    head(asList());
                } else {
                    ret = head(asList());
                }

                return true;
            }
        } else if (m_value.is<Type>()) {
            if constexpr (std::is_same_v<Ret, Void>) {
                head(m_value.as<Type>());
            } else {
                ret = head(m_value.as<Type>());
            }

            return true;
        }

        return overload_(ret, rest...);
    }

    template <typename Ret>
    bool overload_(Ret &ret) const {
        if (m_value.isUndefined()) {
            m_callContext.error("missing required {}", descriptiveName());
            return true;
        }

        return false;
    }

    template <typename Head, typename... Rest>
    void overloadTypeError(std::ostringstream &ss, Head&& head, Rest&... rest) const {
        if (ss.tellp() != 0) {
            ss << ", ";
        }

        using Type = OverloadArgType<Head>;

        if constexpr (std::is_same_v<Type, Argument>) {
            ss << "(any type; this should not be an error)";
        } else if constexpr (std::is_same_v<Type, ArgumentList>) {
            ss << "list";
        } else {
            ss << Value::typeName(Value::typeOf<Type>());
        }

        overloadTypeError(ss, rest...);
    }

    void overloadTypeError(std::ostringstream &ss) const { }
};

class ArgumentList {
public:
    class iterator {
        iterator(const ArgumentList& list, ValueList::const_iterator iter) : m_list(list), m_iter(iter) { }

    public:
        const Argument operator*() const { return m_arg->subValue(*m_iter); }
        bool operator==(const iterator& other) const { return m_iter == other.m_iter; }
        ValueList::const_iterator operator++() { return m_iter++; }

    private:
        const Argument *m_arg;
        const ArgumentList &m_list;
        ValueList::const_iterator m_iter;

        friend class ::ArgumentList;
    };

    ArgumentList() { }
    ArgumentList(const Argument *arg, const ValueList &list) : m_arg(arg), m_list(list) { }

    size_t const size() const {
        return m_list.size();
    }

    Argument at(size_t index) const {
        return m_arg->subValue(m_list.at(index));
    }

    iterator begin() const {
        return iterator(*this, m_list.cbegin());
    }

    iterator end() const {
        return iterator(*this, m_list.cend());
    }

private:
    const Argument *m_arg = nullptr;
    const ValueList &m_list = emptyValueList;
};

inline ArgumentList Argument::asList() const {
    return ArgumentList(this, as<ValueList>());
}

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