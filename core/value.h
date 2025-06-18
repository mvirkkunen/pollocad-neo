#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>
#include <unordered_set>
#include <TopoDS_Shape.hxx>

struct Value;

struct Undefined
{
    bool operator==(const Undefined &) const = default;
};
constexpr const auto undefined = Undefined{};

struct Shape {
    TopoDS_Shape shape;
    std::unordered_set<std::string> tags;

    Shape() { }
    Shape(TopoDS_Shape shape) : shape(shape) { }
    Shape(TopoDS_Shape shape, std::unordered_set<std::string> tags) : shape(shape), tags(tags) { }

    bool operator==(const Shape &) const = default;
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
    void display(std::ostream &os) const;
    std::string type() const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);

    bool operator==(const Value &) const = default;
};
