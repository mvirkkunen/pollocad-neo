#include <iomanip>
#include <sstream>

#include "ast.h"

namespace ast
{

namespace
{

static std::string space(int count) {
    std::stringstream ss;
    ss << std::setw(count * 2) << std::setfill(' ') << "";
    return ss.str();
}

}

bool ExprPtr::operator==(const ExprPtr& other) const {
    return (m_expr == nullptr && other.m_expr == nullptr)
        || (m_expr && other.m_expr && *m_expr == *other.m_expr);
}

void dump(std::ostream &os, const ExprList &expr, int indent) {
    for (const auto &ch : expr) {
        ch.dump(os, indent);
    }
}

void Expr::dump(std::ostream &os, int indent = 0) const {
    std::visit(
        [&os, indent](auto &&ex) {
            using T = std::decay_t<decltype(ex)>;
            if constexpr (std::is_same_v<T, CallExpr>) {
                os << space(indent) << "CallExpr{\n";
                os << space(indent + 1) << "" << ex.func << "\n";
                for (const auto &ch : ex.positional) {
                    ch.dump(os, indent + 1);
                }
                for (const auto &ch : ex.named) {
                    os << space(indent + 1) << ch.first << "=\n";
                    ch.second->dump(os, indent + 2);
                }
                os << space(indent) << "}" << "\n";
            } else if constexpr (std::is_same_v<T, NumberExpr>) {
                os << space(indent) << "NumberExpr{" << ex.value << "}\n";
            } else if constexpr (std::is_same_v<T, StringExpr>) {
                os << space(indent) << "StringExpr{" << std::quoted(ex.value) << "}\n";
            } else if constexpr (std::is_same_v<T, VarExpr>) {
                os << space(indent) << "VarExpr{" << ex.name << "}\n";
            } else if constexpr (std::is_same_v<T, AssignExpr>) {
                os << space(indent) << "AssignExpr{\n";
                os << space(indent + 1) << ex.name << " = \n";
                ex.value->dump(os, indent + 1);
                os << space(indent + 1) << "in:\n";
                ast::dump(os, ex.exprs, indent + 1);
                os << space(indent) << "}\n";
            } else if constexpr (std::is_same_v<T, BlockExpr>) {
                os << space(indent) << "BlockExpr{\n";
                ast::dump(os, ex.exprs, indent + 1);
                os << space(indent) << "}\n";
            } else if constexpr (std::is_same_v<T, ReturnExpr>) {
                os << space(indent) << "return\n";
                ex.expr->dump(os, indent + 1);
            } else {
                static_assert(false, "non-exhaustive visitor!");
            }
        },
        v);
}

std::ostream& operator<<(std::ostream &os, const Expr &expr) {
    expr.dump(os, 0);
    return os;
}

std::ostream& operator<<(std::ostream &os, const ExprList &list) {
    dump(os, list, 0);
    return os;
}

}
