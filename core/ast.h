#pragma once

#include <iostream>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ast {

/*struct CallExpr;
struct NumberExpr;
struct VarExpr;
struct AssignExpr;
struct BlockExpr;*/

struct Expr;

using ExprList = std::vector<Expr>;

class ExprPtr {
public:
    ExprPtr(const ExprPtr& other) : m_expr(other.m_expr) { }
    ExprPtr(const Expr& expr) : m_expr(std::make_shared<Expr>(expr)) { }

    const Expr *operator->() const { return m_expr.get(); }
    Expr *operator->() { return m_expr.get(); }
    const Expr &operator*() const { return *m_expr; }
    Expr &operator*() { return *m_expr; }

    bool operator==(const ExprPtr& other) const;

private:
    std::shared_ptr<Expr> m_expr;
};

struct CallExpr {
    std::string func;
    std::vector<Expr> positional;
    std::unordered_map<std::string, ExprPtr> named;

    bool operator==(const CallExpr&) const = default;
};

struct AssignExpr {
    std::string name;
    ExprPtr value;
    ExprList exprs;

    bool operator==(const AssignExpr&) const = default;
};

struct VarExpr {
    std::string name;

    bool operator==(const VarExpr&) const = default;
};

struct NumberExpr {
    double value;

    bool operator==(const NumberExpr&) const = default;
};

struct BlockExpr {
    ExprList exprs;

    bool operator==(const BlockExpr&) const = default;
};

struct ReturnExpr {
    ExprPtr expr;

    bool operator==(const ReturnExpr&) const = default;
};

struct Expr {
public:
    using Variant = std::variant<CallExpr, NumberExpr, VarExpr, AssignExpr, BlockExpr, ReturnExpr>;

private:
    Variant v;

public:
    Expr(Variant v) : v(std::move(v)) { }
    Expr(CallExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(AssignExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(VarExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(NumberExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(BlockExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(ReturnExpr ex) : Expr(Variant{std::move(ex)}) { }

    const Variant &inner() const { return v; }
    void dump(std::ostream &os, int indent) const;

    friend std::ostream& operator<<(std::ostream &os, const Expr &expr);
    bool operator==(const Expr&) const = default;
};

std::ostream& operator<<(std::ostream &os, const ExprList &list);

} // namespace ast
