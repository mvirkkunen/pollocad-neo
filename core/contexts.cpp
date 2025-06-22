#include "contexts.h"

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
