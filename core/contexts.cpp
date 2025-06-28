#include "contexts.h"

Argument::~Argument() {
    if (m_expectedTypes) {
        if (m_value.isUndefined()) {
            m_callContext.error("missing required argument {}", m_name);
        } else {
            std::stringstream ss;

            bool first = true;
            for (int i = 0; i < sizeof(uint8_t) * CHAR_BIT; i++) {
                if (m_expectedTypes & (1 << i)) {
                    if (first) {
                        first = false;
                    } else {
                        ss << " or ";
                    }

                    ss << Value::typeName(static_cast<Type>(i));
                }
            }

            m_callContext.error("argument {} is of type {}, while the expected type is {}", m_value, m_value.typeName(), ss);
        }
    }
}

const ShapeList CallContext::children() const {
    const auto block = get<Function>("$children");
    if (!block) {
        return {};
    }

    const auto value = (*block)(empty());
    const auto shapes = value.as<ShapeList>();
    if (!shapes) {
        return {};
    }

    return std::move(*shapes);
}

bool Environment::isDefined(const std::string &name) const {
    return m_vars.find(name) != m_vars.end();
}

bool Environment::set(const std::string &name, Value value) {
    if (m_vars.contains(name)) {
        return false;
    }

    m_vars.emplace(name, value);
    return true;
}

bool Environment::setFunction(const std::string &name, Function func) {
    return set(name, Value{func});
}

bool Environment::get(const std::string &name, Value &out) const {
    auto it = m_vars.find(name);
    if (it != m_vars.end()) {
        out = it->second;
        return true;
    }

    if (m_parent) {
        return m_parent->get(name, out);
    }

    out = undefined;
    return false;
}
