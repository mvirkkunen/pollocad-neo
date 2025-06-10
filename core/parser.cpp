#include <optional>

#include "lexy/action/parse.hpp"
#include "lexy/callback.hpp"
#include "lexy/dsl.hpp"
#include "lexy_ext/report_error.hpp"
#include "ast.h"

namespace dsl = lexy::dsl;

namespace
{
namespace grammar
{

constexpr auto ws = dsl::whitespace(dsl::ascii::space);

constexpr auto ident_chars = dsl::identifier(dsl::ascii::alpha_underscore, dsl::ascii::alpha_digit_underscore);

auto kw_if = LEXY_KEYWORD("if", ident_chars);
auto kw_else = LEXY_KEYWORD("else", ident_chars);

constexpr auto ident_class = ident_chars.reserve(kw_if, kw_else);

template <typename T, typename... Rest>
std::vector<T> move_vec(T first, Rest... rest) {
    std::vector<T> vec;
    vec.push_back(std::move(first));
    (vec.push_back(std::move(rest)), ...);
    return vec;
}

struct ident {
    static constexpr auto rule = ident_class;
    static constexpr auto value = lexy::as_string<std::string, lexy::utf8_encoding>;
};

struct stmt_list : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse<struct stmt_list_>;
    static constexpr auto value = lexy::forward<ast::ExprList>;
};

struct expr : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse<struct expr_>;
    static constexpr auto value = lexy::forward<ast::Expr>;
};

struct expr_block {
    static constexpr auto rule = dsl::curly_bracketed(dsl::p<stmt_list>);
    static constexpr auto value = lexy::callback<ast::Expr>([](std::vector<ast::Expr> exprs) { return ast::BlockExpr{std::move(exprs)}; });
};
struct expr_if_else {
    static constexpr auto rule = kw_if >> dsl::p<expr> + dsl::p<expr_block> + kw_else + dsl::p<expr_block>;
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::Expr condition, ast::Expr then, ast::Expr else_) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then), std::move(else_))}; }
    );
};

struct call_args {
    static constexpr auto rule = dsl::parenthesized.opt_list(dsl::opt(dsl::p<ident> >> dsl::equal_sign) + dsl::p<expr>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::fold_inplace<ast::CallExpr>(
        []() { return ast::CallExpr{}; },
        [](auto& expr, lexy::nullopt, ast::Expr value) { expr.positional.push_back(std::move(value)); },
        [](auto& expr, std::string name, ast::Expr value) { expr.named.insert({name, std::move(value)}); }
    ) >> lexy::callback<ast::CallExpr>([](lexy::nullopt) { return ast::CallExpr{}; });
};

struct expr_var_or_call {
    static constexpr auto rule = dsl::p<ident> >> dsl::if_(dsl::p<call_args>);
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](std::string name) { return ast::VarExpr{name}; },
        [](std::string name, ast::CallExpr expr) { return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)}; }
    );
};

/*

struct expr_var_or_call {
    static constexpr auto rule = dsl::p<ident> >> dsl::if_(dsl::p<call_args> >> dsl::if_(dsl::peek(dsl::lit_c<'{'>) >> dsl::p<expr_block>));
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](std::string name) { return ast::VarExpr{name}; },
        [](std::string name, ast::CallExpr expr) { return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)}; },
        [](std::string name, ast::CallExpr expr, ast::Expr children) {
            std::cerr << "found children\n";
            expr.named.insert({"$children", std::move(children)});
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)};
        }
    );
};


 */

/*struct expr_var_or_call_or_assign {
    struct args_ {
        static constexpr auto rule = dsl::parenthesized.list(dsl::opt(dsl::p<ident> >> dsl::equal_sign) + dsl::p<expr>, dsl::sep(dsl::comma));
        static constexpr auto value = lexy::fold_inplace<ast::CallExpr>(
            []() { return ast::CallExpr{}; },
            [](auto& expr, lexy::nullopt, ast::Expr value) { expr.positional.push_back(std::move(value)); },
            [](auto& expr, std::string name, ast::Expr value) { expr.named.insert({name, std::make_shared<ast::Expr>(std::move(value))}); }
            );
    };

    static constexpr auto rule = dsl::p<ident> >> dsl::if_((dsl::equal_sign >> dsl::p<expr> + dsl::semicolon + dsl::p<expr_list>) | dsl::p<args_>);
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](std::string name) { return ast::VarExpr{name}; },
        [](std::string name, ast::Expr value, ast::ExprList exprs) { return ast::AssignExpr{name, std::make_shared<ast::Expr>(std::move(value)), std::move(exprs)}; },
        [](std::string name, ast::CallExpr expr) { return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)}; }
        );
};*/

struct expr_list {
    struct args_ {
        static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::p<expr>, dsl::ignore_trailing_sep(dsl::comma));
        static constexpr auto value = lexy::as_list<std::vector<ast::Expr>>;
    };

    static constexpr auto rule = dsl::p<args_>;
    static constexpr auto value = lexy::callback<ast::Expr>([](std::vector<ast::Expr> args) { return ast::CallExpr{"list", std::move(args)}; });
};

struct expr_number_literal : lexy::token_production {
    static constexpr auto rule = dsl::integer<int>;
    static constexpr auto value = lexy::callback<ast::Expr>([](int value) { return ast::NumberExpr{static_cast<double>(value)}; });
};

struct expr_atom {
    static constexpr auto rule = dsl::parenthesized(dsl::p<expr>)
                                 | dsl::p<expr_block>
                                 | dsl::p<expr_if_else>
                                 | dsl::p<expr_var_or_call>
                                 | dsl::p<expr_list>
                                 | dsl::p<expr_number_literal>;
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::ExprList exprs) { return ast::BlockExpr{std::move(exprs)}; },
        [](ast::Expr expr) { return std::move(expr); }
    );
};

template <char c>
auto op = dsl::op(dsl::lit_c<c>);

constexpr auto op_plus = op<'+'>;
constexpr auto op_minus = op<'-'>;
constexpr auto op_mul = op<'*'>;
constexpr auto op_div = op<'/'>;

struct expr_ : lexy::expression_production
{
    static constexpr auto atom = dsl::p<expr_atom>;

    struct prefix : dsl::prefix_op
    {
        static constexpr auto op = op_minus;
        using operand = dsl::atom;
    };

    struct product : dsl::infix_op_left
    {
        static constexpr auto op = op_mul / op_div;
        using operand = prefix;
    };

    struct sum : dsl::infix_op_left
    {
        static constexpr auto op = op_plus / op_minus;
        using operand = product;
    };

    using operation = sum;

    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::ExprList exprs) { return ast::CallExpr{"-"}; },
        [](ast::Expr expr) { return std::move(expr); },
        [](lexy::op<op_minus>, ast::Expr val) { return ast::CallExpr{"-", move_vec(std::move(val))}; },
        [](ast::Expr lhs, lexy::op<op_mul>, ast::Expr rhs) { return ast::CallExpr{"*", move_vec(std::move(lhs), std::move(rhs))}; },
        [](ast::Expr lhs, lexy::op<op_div>, ast::Expr rhs) { return ast::CallExpr{"/", move_vec(std::move(lhs), std::move(rhs))}; },
        [](ast::Expr lhs, lexy::op<op_plus>, ast::Expr rhs) { return ast::CallExpr{"+", move_vec(std::move(lhs), std::move(rhs))}; },
        [](ast::Expr lhs, lexy::op<op_minus>, ast::Expr rhs) { return ast::CallExpr{"-", move_vec(std::move(lhs), std::move(rhs))}; }
    );
};

struct stmt_if {
    static constexpr auto rule = kw_if + dsl::p<expr> + dsl::p<expr_block> + dsl::if_(kw_else >> dsl::p<expr_block>);
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::Expr condition, ast::Expr then) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then))}; },
        [](ast::Expr condition, ast::Expr then, ast::Expr else_) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then), std::move(else_))}; }
    );
};

struct stmt_let {
    static constexpr auto rule = dsl::p<ident> + dsl::equal_sign + dsl::p<expr> + dsl::semicolon + dsl::p<stmt_list>;
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](std::string name, ast::Expr ex, ast::ExprList exprs) { return ast::AssignExpr{std::move(name), std::move(ex), std::move(exprs)}; }
    );
};

struct stmt_call {
    static constexpr auto rule = dsl::p<ident> + dsl::p<call_args> + (
        dsl::curly_bracketed(dsl::p<stmt_list>)
        | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> dsl::recurse<struct stmt_call>
        | dsl::else_ >> dsl::semicolon
    );
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](std::string name, ast::CallExpr expr) { return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)}; },
        [](std::string name, ast::CallExpr expr, ast::ExprList children) {
            expr.named.emplace("$children", ast::BlockExpr{children});
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)};
        },
        [](std::string name, ast::CallExpr expr, ast::Expr child) {
            expr.named.emplace("$children", ast::BlockExpr{ast::ExprList{{child}}});
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named)};
        }
        /*[](std::string name, ast::CallExpr expr, lexy::nullopt) {
            return ast::NumberExpr{1};
        }*/
    );
};

struct stmt_list_
{
    /*static constexpr auto rule
        = (dsl::peek(dsl::eof | dsl::lit_c<'}'>) >> dsl::nullopt
           | kw_if >> (dsl::p<stmt_if> + dsl::p<stmt_list>)
           | dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> (dsl::p<ident> + dsl::equal_sign + dsl::p<expr> + dsl::semicolon + dsl::p<stmt_list>)
           | dsl::else_ >> (dsl::p<expr> + dsl::opt(dsl::semicolon >> dsl::p<stmt_list>)));

    static constexpr auto value = lexy::callback<ast::ExprList>(
        [](ast::Expr ex) { return ast::ExprList{std::move(ex)}; },
        [](ast::Expr ex, lexy::nullopt) { return ast::ExprList{ast::ReturnExpr{std::move(ex)}}; },
        [](ast::Expr ex, ast::ExprList exs) {
            auto r = ast::ExprList{std::move(ex)};
            std::move(exs.begin(), exs.end(), std::back_inserter(r));
            return r;
        },
        [](ast::ExprList exs) { return exs; },
        [](lexy::nullopt) { return ast::ExprList{}; },
        [](std::string name) { return ast::ExprList{ast::VarExpr{name}}; },
        [](std::string name, ast::Expr ex, ast::ExprList in) {
            return ast::ExprList{ast::AssignExpr{std::move(name), std::move(ex), std::move(in)}};
        });*/

    static constexpr auto rule
        = (dsl::peek(dsl::eof | dsl::lit_c<'}'>) >> dsl::nullopt
           | dsl::peek(kw_if) >> (dsl::p<stmt_if> + dsl::p<stmt_list>)
           | dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::p<stmt_let>
           | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::p<stmt_call> + dsl::p<stmt_list>)
           | dsl::else_ >> (dsl::p<expr> + dsl::if_(dsl::semicolon >> dsl::p<stmt_list>)));

    static constexpr auto value = lexy::callback<ast::ExprList>(
        [](lexy::nullopt) { return ast::ExprList{}; },
        [](ast::Expr ex) { return ast::ExprList{std::move(ex)}; },
        [](ast::Expr ex, ast::ExprList exs) {
            auto r = ast::ExprList{std::move(ex)};
            std::move(exs.begin(), exs.end(), std::back_inserter(r));
            return r;
        },
        [](ast::ExprList exs) { return exs; },
        [](std::string name) { return ast::ExprList{ast::VarExpr{name}}; }
    );
};

struct document {
    static constexpr auto whitespace = dsl::ascii::space;
    static constexpr auto rule = dsl::p<stmt_list> + dsl::eof;
    static constexpr auto value = lexy::forward<std::vector<ast::Expr>>;
};

}
}

std::optional<ast::ExprList> parse(std::string code) {
    auto input = lexy::range_input(code.begin(), code.end());
    auto result = lexy::parse<grammar::document>(input, lexy_ext::report_error);

    if (result.is_error()) {
        std::cout << "Parse error: " << result.errors() << " " << result.has_value() << "\n";
        return std::nullopt;
    }

    return result.value();
}
