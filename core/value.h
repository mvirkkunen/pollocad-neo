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

struct TaggedShape {
    TopoDS_Shape shape;
    std::unordered_set<std::string> tags;

    TaggedShape() { }
    TaggedShape(TopoDS_Shape shape) : shape(shape) { }
    TaggedShape(TopoDS_Shape shape, std::unordered_set<std::string> tags) : shape(shape), tags(tags) { }

    bool operator==(const TaggedShape &) const = default;
};
using TaggedShapes = std::vector<TaggedShape>;

class CallContext;
using FunctionImpl = std::function<Value(const CallContext&)>;
using Function = std::shared_ptr<FunctionImpl>;

using List = std::vector<Value>;

struct RuntimeError {
    std::string error;

    bool operator==(const RuntimeError &) const = default;
};

class Value {
private:
    using Variant = std::variant<Undefined, double, std::string, TaggedShapes, Function, List, RuntimeError>;
    Variant v;

public:
    Value() : v(::undefined) { }

    template <typename T>
    constexpr Value(T v) : v(v) { }

    constexpr Value(int v) : v(static_cast<double>(v)) { }

    template <typename T>
    const T *as() const {
        return std::get_if<T>(&v);
    }

    bool error() const {
        return std::holds_alternative<RuntimeError>(v);
    }

    bool undefined() const {
        return std::holds_alternative<Undefined>(v);
    }

    bool truthy() const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);

    bool operator==(const Value &) const = default;
};
