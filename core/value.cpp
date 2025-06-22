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

namespace
{

static const intptr_t c_numberTag = 0;
static const intptr_t c_integerTag = 1; // TODO 
static const intptr_t c_stringTag = 2;
static const intptr_t c_valueListTag = 3;
static const intptr_t c_shapeListTag = 4;
static const intptr_t c_functionTag = 5;

static const intptr_t c_tagMask = 0x07;
static const intptr_t c_ptrMask = ~c_tagMask;

}

Value::Value(double v) {
    setPtr(c_numberTag, new double(v));
}

Value::Value(std::string v) {
    setPtr(c_stringTag, new std::string(v));
}

Value::Value(ValueList v) {
    setPtr(c_valueListTag, new ValueList(v));
}

Value::Value(ShapeList v) {
    setPtr(c_shapeListTag, new ShapeList(v));
}

Value::Value(Function v) {
    setPtr(c_functionTag, new Function(v));
}

Value::Value(const Value &other) {
    switch (type()) {
        case Type::Undefined:
        case Type::Boolean:
            m_ptr = other.m_ptr;
            break;
        case Type::Number:
            setPtr(c_numberTag, new double(*asUnsafe<double>()));
            break;
        case Type::String:
            setPtr(c_stringTag, new std::string(*asUnsafe<std::string>()));
            break;
        case Type::ValueList:
            setPtr(c_valueListTag, new ValueList(*asUnsafe<ValueList>()));
            break;
        case Type::ShapeList:
            setPtr(c_shapeListTag, new ShapeList(*asUnsafe<ShapeList>()));
            break;
        case Type::Function:
            setPtr(c_functionTag, new Function(*asUnsafe<Function>()));
            break;
        default:
            throw std::runtime_error("corrupted value");
    }
}

bool Value::truthy() const {
    if (m_ptr == c_trueVal) {
        return true;
    }

    switch (type()) {
        case Type::Number:
            return *asUnsafe<double>() != 0.0;
        case Type::String:
            return !asUnsafe<std::string>()->empty();
        case Type::ValueList:
            return !asUnsafe<ValueList>()->empty();
        case Type::ShapeList:
            return !asUnsafe<ShapeList>()->empty();
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
            return os << ((m_ptr == c_trueVal) ? "true" : "false");
        case Type::Number:
            return os << *asUnsafe<double>();
        case Type::String:
            return os << std::quoted(*asUnsafe<std::string>());
        case Type::ValueList: {
            const auto *plist = asUnsafe<ValueList>();
            os << "[";

            bool first = true;
            for (const auto& item : *plist) {
                if (first) {
                    first = false;
                } else {
                    os << ", ";
                }

                item.repr(os);
            }
            os << "]";
        }
        case Type::ShapeList:
            return os << "{Shape}";
        case Type::Function:
            return os << "{Function}";
    }

    throw std::runtime_error("corrupted value");
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
            return os << *asUnsafe<std::string>();
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
    if (type() != other.type()) {
        return false;
    }

    switch (type()) {
        case Type::Undefined:
        case Type::Boolean:
            return m_ptr == other.m_ptr;
        case Type::Number:
            return *asUnsafe<double>() == *other.asUnsafe<double>();
        case Type::String:
            return *asUnsafe<std::string>() == *other.asUnsafe<std::string>();
        case Type::ValueList:
            return *asUnsafe<ValueList>() == *other.asUnsafe<ValueList>();
        case Type::ShapeList:
            return *asUnsafe<ShapeList>() == *other.asUnsafe<ShapeList>();
        case Type::Function:
            return asUnsafe<Function>() == other.asUnsafe<Function>();
    }

    throw std::runtime_error("corrupted value");
}

Type Value::type() const {
    if (m_ptr <= c_tagMask) {
        switch (m_ptr) {
            case c_undefinedVal: return Type::Undefined;
            case c_trueVal: case c_falseVal: return Type::Boolean;
        }
    } else {
        switch (m_ptr & c_tagMask) {
            case c_numberTag: return Type::Number;
            case c_stringTag: return Type::String;
            case c_valueListTag: return Type::ValueList;
            case c_shapeListTag: return Type::ShapeList;
            case c_functionTag: return Type::Function;
        }
    }
    
    throw std::runtime_error("corrupted value");
}

void Value::setPtr(intptr_t tag, void *ptr) {
    std::cerr << "ptr= " << ptr << " " << tag << "\n";
    m_ptr = reinterpret_cast<intptr_t>(ptr) | tag;
}

void *Value::getPtr() const {
    static Undefined undefinedVal{};
    static bool falseVal = false;
    static bool trueVal = true;

    if (m_ptr <= c_tagMask) {
        switch (m_ptr) {
            case c_undefinedVal: return &undefinedVal;
            case c_trueVal: return &trueVal;
            case c_falseVal: return &falseVal;
        }

        throw std::runtime_error("corrupted value");
    }

    auto ptr = reinterpret_cast<void *>(m_ptr & c_ptrMask);

    std::cerr << "ptr2=" << ptr << " type=" << static_cast<int>(type()) << "\n";

    return ptr;
}

void Value::deletePtr() {
    switch (type()) {
        case Type::Number:
            delete asUnsafe<double>();
        case Type::String:
            delete asUnsafe<std::string>();
        case Type::ValueList:
            delete asUnsafe<ValueList>();
        case Type::ShapeList:
            delete asUnsafe<ShapeList>();
        case Type::Function:
            delete asUnsafe<Function>();
    }
}
