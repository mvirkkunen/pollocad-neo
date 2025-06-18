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
constexpr const auto undefined = Undefined{};

class Shape {
public:
    Shape() { }
    Shape(TopoDS_Shape shape, Span span={});
    Shape(TopoDS_Shape shape, std::unordered_map<std::string, Value> props, std::vector<Span> spans);

    Shape withShape(TopoDS_Shape shape, Span span={}) const;
    Shape withProp(const std::string &name, const Value &value) const;

    const TopoDS_Shape &shape() const { return m_shape; }
    bool hasProp(const std::string &name) const;
    Value getProp(const std::string &name) const;

    bool operator==(const Shape &) const = default;

private:
    TopoDS_Shape m_shape;
    std::unordered_map<std::string, Value> m_props;
    std::vector<Span> m_spans;
};
using ShapeList = std::vector<Shape>;

class CallContext;
using FunctionImpl = std::function<Value(const CallContext&)>;
using Function = std::shared_ptr<FunctionImpl>;

using List = std::vector<Value>;

class Value {
private:
    using Variant = std::variant<Undefined, double, std::string, ShapeList, Function, List>;
    Variant v;

public:
    Value() : v(::undefined) { }

    constexpr Value(Undefined) : v(::undefined) { }
    constexpr Value(double v) : v(v) { }
    constexpr Value(int v) : v(static_cast<double>(v)) { }
    constexpr Value(const std::string v) : v(v) { }
    constexpr Value(ShapeList v) : v(v) { }
    constexpr Value(List v) : v(v) { }
    Value(Function v) : v(v) { }

    template <typename T>
    const T *as() const {
        return std::get_if<T>(&v);
    }

    bool undefined() const { return std::holds_alternative<Undefined>(v); }
    bool truthy() const;
    std::string display() const;
    void display(std::ostream &os) const;
    std::string type() const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);

    bool operator==(const Value &) const = default;
};
