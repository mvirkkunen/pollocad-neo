#pragma once

#include <iostream>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "logmessage.h"
#include "value.h"

namespace ast {

struct Expr;

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

struct BlockExpr {
    std::vector<Expr> exprs;
    Span span;

    BlockExpr() { }
    BlockExpr(std::vector<Expr> exprs) : exprs(exprs) { }
    BlockExpr(std::initializer_list<Expr> exprs) : exprs(exprs) { }

    bool operator==(const BlockExpr&) const = default;
};

struct LiteralExpr {
    std::shared_ptr<Value> value = nullptr;

    template <typename T>
    LiteralExpr(const T& value) : value(std::make_shared<Value>(value)) { }
    //LiteralExpr(const Value& value) : value(std::make_shared<Value>(value)) { }
    //LiteralExpr(double value) : value(std::make_shared<Value>(value)) { }

    bool operator==(const LiteralExpr& other) const { return *value == *other.value; }
};

struct VarExpr {
    std::string name;
    Span span;

    bool operator==(const VarExpr&) const = default;
};

struct LetExpr {
    std::string name;
    ExprPtr value;
    bool return_;
    Span span;

    bool operator==(const LetExpr&) const = default;
};

struct CallExpr {
    std::string func;
    std::vector<Expr> positional;
    std::unordered_map<std::string, ExprPtr> named;
    Span span;

    bool operator==(const CallExpr&) const = default;
};

struct LambdaExpr {
    struct Arg {
        std::string name;
        std::optional<ExprPtr> default_;

        bool operator==(const Arg&) const = default;
    };

    ExprPtr body;
    std::vector<Arg> args;
    std::string name;
    Span span;

    bool operator==(const LambdaExpr&) const = default;
};

struct Expr {
public:
    using Variant = std::variant<BlockExpr, LiteralExpr, VarExpr, LetExpr, CallExpr, LambdaExpr>;

private:
    Variant v;

public:
    Expr(const Expr &) = default;
    Expr(Variant v) : v(std::move(v)) { }
    Expr(BlockExpr v) : v(Variant{std::move(v)}) { }
    Expr(LiteralExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(VarExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(LetExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(CallExpr ex) : Expr(Variant{std::move(ex)}) { }
    Expr(LambdaExpr ex) : Expr(Variant{std::move(ex)}) { }

    Variant &inner() { return v; }
    const Variant &cinner() const { return v; }
    void dump(std::ostream &os, int indent) const;

    friend std::ostream& operator<<(std::ostream &os, const Expr &expr);
    bool operator==(const Expr&) const = default;
};

} // namespace ast
