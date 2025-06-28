#include "contexts.h"

Argument CallContext::arg(const char *name) {
    const auto &value = (m_nextPositional < m_positional.size()) ? m_positional.at(m_nextPositional) : undefined;
    m_nextPositional++;
    return Argument(*this, name, value);
}

Argument CallContext::named(const char *name) const {
    const auto &it = m_named.find(name);
    const auto &value = (it != m_named.end() ? it->second : undefined);
    return Argument(*this, name, value);
}

const ShapeList CallContext::children() const {
    const auto block = named("$children").asAny();
    if (!block) {
        return {};
    }

    auto ec = empty();
    return block.as<Function>()(ec).as<ShapeList>();
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
