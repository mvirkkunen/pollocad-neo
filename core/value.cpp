#include <cassert>

#include "value.h"

Shape::Shape(TopoDS_Shape shape, Span span) : m_shape(shape), m_spans({span}) { }

Shape::Shape(TopoDS_Shape shape, std::vector<Span> spans) : m_shape(shape), m_spans(spans) { }

Shape::Shape(TopoDS_Shape shape, std::unordered_map<std::string, Value> props, std::vector<Span> spans) : m_shape(shape), m_props(props), m_spans(spans) { }

Shape Shape::withShape(TopoDS_Shape shape, Span span) const {
    std::vector<Span> newSpans = m_spans;
    if (!span.isEmpty()) {
        newSpans.push_back(span);
    }

    return Shape(shape, m_props, newSpans);
}

Shape Shape::withProp(const std::string &name, const Value &value) const {
    std::unordered_map<std::string, Value> newProps = m_props;
    newProps.emplace(name, value);
    
    return Shape(m_shape, newProps, m_spans);
}

bool Shape::hasProp(const std::string &name) const {
    return m_props.contains(name);
}

Value Shape::getProp(const std::string &name) const {
    auto it = m_props.find(name);
    return (it == m_props.end()) ? undefined : it->second;
}

Value Value::trueValue = Value::constructBool(true);
Value Value::falseValue = Value::constructBool(false);

Value::Value(bool v) : Value(v ? trueValue : falseValue) { }

Value::Value(double v) {
    auto bits = std::rotl(~std::bit_cast<uint64_t>(v), c_rotate);
    if ((bits & 0x7) == 0) {
        // Unusual double, must represent as Cell
        constructCell(v);
    } else {
        m_value = bits;
    }
}

Value::Value(std::string v) { constructCell(v); }
Value::Value(ValueList v) { constructCell(v); }
Value::Value(ShapeList v) { constructCell(v); }
Value::Value(Function v) { constructCell(v); }

Value::Value(const Value &other) { *this = other; }

Value &Value::operator=(const Value &other) {
    m_value = other.m_value;
    if (isCell()) {
        getCellUnsafe()->refCount.fetch_add(1);
    }

    return *this;
}

bool Value::isTruthy() const {
    if (m_value == c_undefinedVal) {
        return false;
    }

    switch (type()) {
        case Type::Boolean:
            return getCellTUnsafe<bool>()->value;
        case Type::Number:
            return asDouble() != 0.0;
        case Type::String:
            return !getCellTUnsafe<std::string>()->value.empty();
        case Type::ValueList:
            return !getCellTUnsafe<ValueList>()->value.empty();
        case Type::ShapeList:
            return !getCellTUnsafe<ShapeList>()->value.empty();
        case Type::Function:
            return true;
    }

    return false;
}

std::ostream &Value::repr(std::ostream &os) const {
    switch (type()) {
        case Type::Undefined:
            return os << "undefined";
        case Type::Boolean:
            return os << (as<bool>() ? "true" : "false");
        case Type::Number:
            return os << as<double>();
        case Type::String:
            return os << std::quoted(as<std::string>());
        case Type::ValueList: {
            ValueList list = as<ValueList>();
            os << "[";

            bool first = true;
            for (const auto& item : list) {
                if (first) {
                    first = false;
                } else {
                    os << ", ";
                }

                item.repr(os);
            }
            os << "]";
            break;
        }
        case Type::ShapeList:
            return os << "{shape}";
        case Type::Function:
            return os << "{function}";
    }

    assert("repr: corrupted Cell");
    return os;
}

std::string Value::repr() const {
    std::ostringstream ss;
    repr(ss);
    return ss.str();
}

std::ostream &Value::display(std::ostream &os) const {
    switch (type()) {
        case Type::Undefined:
            return os;
        case Type::String:
            return os << as<std::string>();
        default:
            return repr(os);
    }
}

std::string Value::display() const {
    std::ostringstream ss;
    display(ss);
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const Value& val) {
    return val.repr(os);
}

bool Value::operator==(const Value &other) const {
    if (m_value == other.m_value) {
        return true;
    }

    if (type() != other.type()) {
        return false;
    }

    if (!isCell()) {
        assert("operator==: corrupted Value");
        return false;
    }

    switch (type()) {
        case Type::Boolean: return isCellEqualUnsafe<bool>(other);
        case Type::Number: return isCellEqualUnsafe<double>(other);
        case Type::String: return isCellEqualUnsafe<std::string>(other);
        case Type::ValueList: return isCellEqualUnsafe<ValueList>(other);
        case Type::ShapeList: return isCellEqualUnsafe<ShapeList>(other);
        case Type::Function: return false; // functions are handled well enough by m_value equality check above
    }

    assert("operator==: corrupted Cell");
    return false;
}

Type Value::type() const {
    return !m_value
        ? Type::Undefined
        : isCell()
        ? getCellUnsafe()->type
        : Type::Number;
}

double Value::asDouble() const {
    if (m_value == c_undefinedVal) {
        return 0.0;
    }

    if (isCell()) {
        return (getCellUnsafe()->type == Type::Number) ? getCellTUnsafe<double>()->value : 0.0;
    }

    return std::bit_cast<double>(~std::rotr(m_value, c_rotate));
}

template <typename T>
void Value::constructCell(T v) {
    static_assert(sizeof(void *) <= sizeof(m_value));
    m_value = reinterpret_cast<uint64_t>(static_cast<Cell *>(new CellT<T>{1, typeOf<T>(), v}));
}

// Safety contract: both Values must be Cells of type T
template <typename T>
bool Value::isCellEqualUnsafe(const Value &other) const {
    return getCellTUnsafe<T>()->value == other.getCellTUnsafe<T>()->value;
}

// Safety contract: Value must be a Cell
Value::Cell *Value::getCellUnsafe() const {
    return reinterpret_cast<Cell *>(m_value);
}

// Safety contract: Value must be a Cell of type T
template <typename T>
Value::CellT<T> const *Value::getCellTUnsafe() const {
    return static_cast<CellT<T> *>(getCellUnsafe());
}

// Safety contract: Value must be a Cell
void Value::releaseCellUnsafe() {
    if (getCellUnsafe()->refCount.fetch_sub(1) == 1) {
        switch (getCellUnsafe()->type) {
            case Type::Boolean:
                assert("Booleans should never be deleted");
                break;
            case Type::Number:
                delete getCellTUnsafe<double>();
                break;
            case Type::String:
                delete getCellTUnsafe<std::string>();
                break;
            case Type::ValueList:
                delete getCellTUnsafe<ValueList>();
                break;
            case Type::ShapeList:
                delete getCellTUnsafe<ShapeList>();
                break;
            case Type::Function:
                delete getCellTUnsafe<Function>();
                break;
            default:
                assert("releaseCellUnsafe: corrupted Cell");
                break;
        }
    }
}

Value Value::constructBool(bool v) {
    Value value;
    value.constructCell(v);
    return value;
}
