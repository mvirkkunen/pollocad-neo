#include "value.h"

Shape::Shape(TopoDS_Shape shape, Span span) : m_shape(shape), m_spans({span}) { }

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

bool Value::truthy() const {
    return std::visit(
        [](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, double>) {
                return v != 0.0;
            } else if constexpr (std::is_same_v<T, Function>) {
                return true;
            } else if constexpr (std::is_same_v<T, std::vector<Value>>) {
                return !v.empty();
            } else {
                return false;
            }
        },
        v);
}

std::ostream& operator<<(std::ostream& os, const Value& val) {
    std::visit(
        [&os](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Undefined>) {
                os << "undefined";
            } else if constexpr (std::is_same_v<T, double>) {
                os << v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                os << std::quoted(v);
            } else if constexpr (std::is_same_v<T, ShapeList>) {
                os << "{ShapeList}";
            } else if constexpr (std::is_same_v<T, Function>) {
                os << "{Function}";
            } else if constexpr (std::is_same_v<T, std::vector<Value>>) {
                os << "[";

                bool first = true;
                for (const auto& c : v) {
                    if (first) {
                        first = false;
                    } else {
                        os << ", ";
                    }

                    os << c;
                }
                os << "]";
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        val.v);

    return os;
}

void Value::display(std::ostream &os) const {
    std::visit(
        [&os](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Undefined>) {
                os << "{Undefined}";
            } else if constexpr (std::is_same_v<T, double>) {
                os << v;
            } else if constexpr (std::is_same_v<T, std::string>) {
                os << v;
            } else if constexpr (std::is_same_v<T, ShapeList>) {
                os << "{Shapes}";
            } else if constexpr (std::is_same_v<T, Function>) {
                os << "{Function}";
            } else if constexpr (std::is_same_v<T, std::vector<Value>>) {
                bool first = true;

                os << "[";
                for (const auto& c : v) {
                    if (first) {
                        first = false;
                    } else {
                        os << ", ";
                    }

                    c.display(os);
                }
                os << "]";
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        v);
}

std::string Value::type() const {
    return std::visit(
        [](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, Undefined>) {
                return "undefined";
            } else if constexpr (std::is_same_v<T, double>) {
                return "number";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "string";
            } else if constexpr (std::is_same_v<T, ShapeList>) {
                return "shapelist";
            } else if constexpr (std::is_same_v<T, Function>) {
                return "function";
            } else if constexpr (std::is_same_v<T, std::vector<Value>>) {
                return "list";
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        v);
}
