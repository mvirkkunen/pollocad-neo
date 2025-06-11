#include "value.h"

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
            } else if constexpr (std::is_same_v<T, TaggedShapes>) {
                os << "{TaggedShapes}";
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
            } else if constexpr (std::is_same_v<T, RuntimeError>) {
                os << "{RuntimeError: " << v.error << "}\n";
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        val.v);

    return os;
}
