#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_map>
#include <TopoDS_Shape.hxx>

#include "logmessage.h"

struct Value;

struct Undefined
{
    bool operator==(const Undefined &) const = default;
};

using ValueList = std::vector<Value>;

class Shape {
public:
    Shape() { }
    Shape(TopoDS_Shape shape, Span span={});
    Shape(TopoDS_Shape shape, std::vector<Span> spans);
    Shape(TopoDS_Shape shape, std::unordered_map<std::string, Value> props, std::vector<Span> spans);

    Shape withShape(TopoDS_Shape shape, Span span={}) const;
    Shape withProp(const std::string &name, const Value &value) const;

    const TopoDS_Shape &shape() const { return m_shape; }
    bool hasProp(const std::string &name) const;
    Value getProp(const std::string &name) const;

    const std::vector<Span> &spans() const { return m_spans; }

    bool operator==(const Shape &) const = default;

private:
    TopoDS_Shape m_shape;
    std::unordered_map<std::string, Value> m_props;
    std::vector<Span> m_spans;
};
using ShapeList = std::vector<Shape>;

class CallContext;
using Function = std::function<Value(const CallContext&)>;

enum class Type {
    Undefined = 0,
    Boolean = 1,
    Number = 2,
    String = 3,
    ValueList = 4,
    ShapeList = 5,
    Function = 6,
};

class alignas(8) Value {
public:
    constexpr Value() : Value(Undefined{}) { }
    constexpr Value(Undefined) : m_ptr(c_undefinedVal) { }
    constexpr Value(bool b) : m_ptr(b ? c_trueVal : c_falseVal) { }
    Value(double v);
    Value(int v) : Value(static_cast<double>(v)) { }
    Value(std::string v);
    Value(ValueList v);
    Value(ShapeList v);
    Value(Function v);
    Value(const Value &other);
    //constexpr Value::Value(const Value &&other) : m_ptr(std::move(other.m_ptr)) { }

    constexpr ~Value() {
        if (m_ptr != c_undefinedVal && m_ptr != c_trueVal && m_ptr != c_falseVal) {
            deletePtr();
        }
    }

    Type type() const;
    const char *typeName() const { return typeName(type()); }

    template <typename T>
    const T *as() const {
        return (type() == typeOf<T>()) ? asUnsafe<T>() : nullptr;
    }

    constexpr bool undefined() const { return m_ptr == c_undefinedVal; }
    bool truthy() const;

    std::ostream &repr(std::ostream &os) const;
    std::string repr() const;

    std::ostream &display(std::ostream &os) const;
    std::string display() const;

    bool operator==(const Value &other) const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);

    template <typename T>
    static Type typeOf() {
        if constexpr (std::is_same_v<T, Undefined>) {
            return Type::Undefined;
        } else if constexpr (std::is_same_v<T, bool>) {
            return Type::Boolean;
        } else if constexpr (std::is_same_v<T, double>) {
            return Type::Number;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Type::String;
        } else if constexpr (std::is_same_v<T, ShapeList>) {
            return Type::ShapeList;
        } else if constexpr (std::is_same_v<T, Function>) {
            return Type::Function;
        } else if constexpr (std::is_same_v<T, ValueList>) {
            return Type::ValueList;
        } else {
            static_assert(false, "this type cannot be represented");
        }
    }

    static const char *typeName(Type type) {
        const char *typeNames[] = {
            "undefined",
            "bool",
            "number",
            "string",
            "shape",
            "function",
            "list"
        };

        const auto index = static_cast<size_t>(type);
        return typeNames[index < sizeof(typeNames)/sizeof(typeNames[0]) ? index : 0];
    }

private:
    static const intptr_t c_undefinedVal = 0;
    static const intptr_t c_falseVal = 1;
    static const intptr_t c_trueVal = 2;

    intptr_t m_ptr;

    void setPtr(intptr_t tag, void *ptr);
    void *getPtr() const;
    void deletePtr();

    template <typename T>
    T *asUnsafe() const {
        return reinterpret_cast<T *>(getPtr());
    }
};

constexpr const auto undefined = Value{};

