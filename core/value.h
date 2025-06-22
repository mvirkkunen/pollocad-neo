#pragma once

#include <atomic>
#include <bit>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
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

template <typename T> struct OptionalValueT { using Type = const T *; };
template <> struct OptionalValueT<bool> { using Type = std::optional<bool>; };
template <> struct OptionalValueT<double> { using Type = std::optional<double>; };

template <typename T> using OptionalValue = OptionalValueT<T>::Type;

class alignas(8) Value {
public:
    constexpr Value() : m_value(c_undefinedVal) { }
    constexpr Value(Undefined) : m_value(c_undefinedVal) { }
    Value(bool v);
    Value(double v);
    Value(int64_t v) : Value(static_cast<double>(v)) { }
    Value(int v) : Value(static_cast<double>(v)) { }
    Value(std::string v);
    Value(const char *v) : Value(std::string(v)) { }
    Value(ValueList v);
    Value(ShapeList v);
    Value(Function v);
    Value(const Value &other);
    Value &operator=(const Value &other);

    constexpr ~Value() {
        if (isCell()) {
            releaseCellUnsafe();
        }
    }

    Type type() const;
    const char *typeName() const { return typeName(type()); }

    template <typename T>
    OptionalValue<T> as() const {
        if constexpr (std::is_same_v<T, bool>) {
            return (type() == Type::Boolean) ? std::optional<bool>(getCellTUnsafe<T>()->value) : std::nullopt;
        } else if constexpr (std::is_same_v<T, double>) {
            return asDouble();
        } else {
            return (type() == typeOf<T>()) ? &getCellTUnsafe<T>()->value : nullptr;
        }
    }

    template <typename T>
    const T asOrDefault(T default_) const {
        auto value = as<T>();
        return value ? *value : default_;
    }

    constexpr bool undefined() const { return m_value == c_undefinedVal; }
    bool truthy() const;

    std::ostream &repr(std::ostream &os) const;
    std::string repr() const;

    std::ostream &display(std::ostream &os) const;
    std::string display() const;

    bool operator==(const Value &other) const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);

    template <typename T> static Type typeOf() {
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
            "list",
            "shape",
            "function",
        };

        const auto index = static_cast<size_t>(type);
        return typeNames[index < sizeof(typeNames)/sizeof(typeNames[0]) ? index : 0];
    }

private:
    static const uint64_t c_undefinedVal = 0;

    static const int c_rotate = 4;

    static Value trueValue;
    static Value falseValue;

    // Value representation:
    //
    // - if m_value == 0 -> undefined
    // - else if m_value low 3 bits are 000 -> pointer to Cell (might store a double if it was an unlikely value)
    // - else m_value = ~rotl(doubleVal, c_rotate) -> the bit fudging makes it very unlikely for the values to lool like a pointer

    struct Cell {
        std::atomic_uint32_t refCount;
        Type type;
    };

    template <typename T>
    struct CellT : public Cell {
        T value;
    };

    uint64_t m_value;

    std::optional<double> asDouble() const;

    template <typename T>
    void constructCell(T v);

    constexpr bool isCell() const { return m_value && (m_value & 0x7) == 0; }

    template <typename T>
    bool isCellEqualUnsafe(const Value &other) const;

    Cell *getCellUnsafe() const;

    template <typename T>
    CellT<T> const *getCellTUnsafe() const;

    void releaseCellUnsafe();
    
    static Value constructBool(bool v);
};

constexpr const auto undefined = Value{};

