#include <sstream>

#include "lexy/action/parse.hpp"
#include "lexy/callback.hpp"
#include "lexy/dsl.hpp"
#include "lexy_ext/report_error.hpp"

#include "parser.h"

namespace dsl = lexy::dsl;

using Input = lexy::range_input<lexy::utf8_encoding, std::string::const_iterator>;
using Pos = std::string::const_iterator;

template <typename Input, typename Iterator>
Span getSpan(const Input &input, const Iterator &start, const Iterator &end) {
    auto loc = lexy::get_input_location(input, start);
    return Span{
        static_cast<int>(start - input.begin()),
        static_cast<int>(end - input.begin()),
        static_cast<int>(loc.line_nr()),
        static_cast<int>(loc.column_nr())
    };
}

struct ParseState {
    const Input &input;
    bool enableSpans;

    Span span(const Pos &start, const Pos &end) {
        if (!enableSpans) {
            return Span{};
        }

        return getSpan(input, start, end);
    }
};

template <typename Ret, typename... Args>
constexpr auto state_callback(Args... args) {
    return lexy::bind(lexy::callback<Ret>(args...), lexy::parse_state, lexy::values);
}

namespace grammar
{

constexpr auto ws_rule = dsl::ascii::space | (LEXY_LIT("/*") >> dsl::until(LEXY_LIT("*/"))) | (LEXY_LIT("//") >> dsl::until(dsl::newline).or_eof());

constexpr auto ws = dsl::whitespace(ws_rule);

constexpr auto ident_chars = dsl::identifier(dsl::ascii::alpha_underscore / dsl::dollar_sign, dsl::ascii::alpha_digit_underscore);

auto kw_if = LEXY_KEYWORD("if", ident_chars);
auto kw_else = LEXY_KEYWORD("else", ident_chars);
auto kw_def = LEXY_KEYWORD("def", ident_chars);

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
    static constexpr auto value = lexy::as_string<std::string>;
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
    static constexpr auto value = lexy::callback<ast::Expr>([](std::vector<ast::Expr> exprs) { return ast::LambdaExpr{std::move(exprs)}; });
};

struct expr_if_else {
    static constexpr auto rule = kw_if >> dsl::p<expr> + dsl::p<expr_block> + kw_else + dsl::p<expr_block>;
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::Expr condition, ast::Expr then, ast::Expr else_) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then), std::move(else_))}; }
    );
};

struct call_args {
    static constexpr const char *name = "function call arguments";

    static constexpr auto rule = dsl::parenthesized.opt_list(dsl::opt(dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::p<ident> + dsl::equal_sign) + dsl::p<expr>, dsl::sep(dsl::comma));
    static constexpr auto value = lexy::fold_inplace<ast::CallExpr>(
        []() { return ast::CallExpr{}; },
        [](auto& expr, lexy::nullopt, ast::Expr value) { expr.positional.push_back(std::move(value)); },
        [](auto& expr, std::string name, ast::Expr value) { expr.named.insert({name, std::move(value)}); }
    ) >> lexy::callback<ast::CallExpr>([](lexy::nullopt) { return ast::CallExpr{}; });
};

struct expr_var_or_call {
    static constexpr const char *name = "function call expression";

    static constexpr auto rule = dsl::position(dsl::p<ident> >> dsl::position) >> dsl::if_(dsl::p<call_args>);
    static constexpr auto value = state_callback<ast::Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end) { return ast::VarExpr{name, st.span(begin, end)}; },
        [](ParseState &st, Pos begin, std::string name, Pos end, ast::CallExpr expr) { return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)}; }
    );
};

struct expr_list {
    struct args_ {
        static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::p<expr>, dsl::ignore_trailing_sep(dsl::comma));
        static constexpr auto value = lexy::as_list<std::vector<ast::Expr>>;
    };

    static constexpr auto rule = dsl::p<args_>;
    static constexpr auto value = lexy::callback<ast::Expr>([](std::vector<ast::Expr> args) { return ast::CallExpr{"list", std::move(args)}; });
};

struct expr_number_literal : lexy::token_production {
    static constexpr auto rule = dsl::peek(dsl::sign + ws + dsl::digit<>) >> dsl::capture(dsl::token(dsl::sign + dsl::digits<> + dsl::opt(dsl::period >> dsl::digits<>)));
    static constexpr auto value = lexy::as_string<std::string>
        | lexy::callback<ast::Expr>([](std::string value) {
            double n;
            std::istringstream(value) >> n;
            return ast::LiteralExpr{n};
        });
};

struct expr_string_literal : lexy::token_production {
    static constexpr auto rule = dsl::quoted(-dsl::unicode::control);
    static constexpr auto value = lexy::as_string<std::string>
        >> lexy::callback<ast::Expr>([](std::string value) { return ast::LiteralExpr{value}; });
};

struct expr_atom {
    struct PropertyAccess {
        std::variant<ast::Expr, std::string> index;
        Span span;
    };

    static constexpr auto name = "expression atom";

    struct expected_expression_error {
        static constexpr auto name = "expected expression";
    };

    struct bracketed_index_op_ : lexy::transparent_production {
        static constexpr auto name = "indexing expression";
        static constexpr auto rule = dsl::square_bracketed(dsl::position + dsl::p<expr> + dsl::position);
        static constexpr auto value = state_callback<PropertyAccess>(
            [](ParseState &st, Pos begin, ast::Expr index, Pos end) { return PropertyAccess{std::move(index), st.span(begin, end)}; }
        );
    };

    struct property_access_op_ : lexy::transparent_production {
        static constexpr auto name = "property access expression";
        static constexpr auto rule = dsl::lit_c<'.'> >> (dsl::position + dsl::p<ident> + dsl::position);
        static constexpr auto value = state_callback<PropertyAccess>(
            [](ParseState &st, Pos begin, std::string index, Pos end) { return PropertyAccess{index, st.span(begin, end)}; }
        );
    };

    struct index_ops_ {
        //static constexpr auto rule = dsl::list(dsl::square_bracketed(dsl::p<expr>) | dsl::lit_c<'.'> >> dsl::p<ident>);
        static constexpr auto rule = dsl::list(dsl::p<bracketed_index_op_> | dsl::p<property_access_op_>);
        static constexpr auto value = lexy::as_list<std::vector<PropertyAccess>>;
    };

    static constexpr auto rule = (dsl::parenthesized(dsl::p<expr>)
                                  //| dsl::p<expr_block>
                                  | dsl::p<expr_if_else>
                                  | dsl::p<expr_var_or_call>
                                  | dsl::p<expr_list>
                                  | dsl::p<expr_number_literal>
                                  | dsl::p<expr_string_literal>
                                  | dsl::error<expected_expression_error>) + dsl::opt(dsl::p<index_ops_>);
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::Expr expr, lexy::nullopt = {}) { return std::move(expr); },
        [](ast::Expr expr, std::vector<PropertyAccess> access) {
            // TODO: maybe this could be a fold
            for (const auto &a : access) {
                std::visit<void>(
                    [&expr, &a](const auto &v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, ast::Expr>) {
                            expr = ast::CallExpr{"[]", {expr, v}, {}, a.span};
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            expr = ast::CallExpr{"[]", {expr, ast::LiteralExpr{v}}, {}, a.span};
                        } else {
                            static_assert(false, "non-exhaustive visitor!");
                        }
                    },
                    a.index);
            }

            return expr;
        }
    );
};

template <char c>
auto op = dsl::op(dsl::lit_c<c>);

constexpr auto op_plus = op<'+'>;
constexpr auto op_minus = op<'-'>;
constexpr auto op_mul = op<'*'>;
constexpr auto op_div = op<'/'>;
constexpr auto op_mod = op<'%'>;

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
        static constexpr auto op = op_mul / op_div / op_mod;
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
        [](ast::Expr lhs, lexy::op<op_mod>, ast::Expr rhs) { return ast::CallExpr{"%", move_vec(std::move(lhs), std::move(rhs))}; },
        [](ast::Expr lhs, lexy::op<op_plus>, ast::Expr rhs) { return ast::CallExpr{"+", move_vec(std::move(lhs), std::move(rhs))}; },
        [](ast::Expr lhs, lexy::op<op_minus>, ast::Expr rhs) { return ast::CallExpr{"-", move_vec(std::move(lhs), std::move(rhs))}; }
    );
};

struct stmt_if {
    static constexpr auto name = "if statement";
    static constexpr auto rule = kw_if + dsl::p<expr> + dsl::p<expr_block> + dsl::if_(kw_else >> dsl::p<expr_block>);
    static constexpr auto value = lexy::callback<ast::Expr>(
        [](ast::Expr condition, ast::Expr then) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then))}; },
        [](ast::Expr condition, ast::Expr then, ast::Expr else_) { return ast::CallExpr{"if", move_vec(std::move(condition), std::move(then), std::move(else_))}; }
    );
};

struct stmt_let {
    static constexpr auto name = "assignment statement";
    static constexpr auto rule = dsl::position + dsl::p<ident> + dsl::position + dsl::equal_sign + dsl::p<expr>;
    static constexpr auto value = state_callback<ast::Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end, ast::Expr ex) { return ast::LetExpr{std::move(name), std::move(ex), st.span(begin, end)}; }
    );
};

struct stmt_call {
    static constexpr auto name = "function call statement";
    static constexpr auto rule = dsl::position(dsl::p<ident>) + dsl::position + dsl::p<call_args> + (
        dsl::curly_bracketed(dsl::p<stmt_list>)
        | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> dsl::recurse<struct stmt_call>
        | dsl::else_ >> dsl::peek(dsl::semicolon | dsl::lit_c<'}'> | dsl::eof)
    );
    static constexpr auto value = state_callback<ast::Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end, ast::CallExpr expr) {
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)};
        },
        [](ParseState &st, Pos begin, std::string name, Pos end, ast::CallExpr expr, ast::ExprList children) {
            expr.named.emplace("$children", ast::LambdaExpr{children});
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)};
        },
        [](ParseState &st, Pos begin, std::string name, Pos end, ast::CallExpr expr, ast::Expr child) {
            expr.named.emplace("$children", ast::LambdaExpr{ast::ExprList{{child}}});
            return ast::CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)};
        }
    );
};

struct stmt_def {
    struct args_ {
        static constexpr auto rule = [] {
            auto seen_named = dsl::context_flag<stmt_def>;
            // TODO ban positional after named

            return dsl::parenthesized.opt_list(dsl::p<ident> + dsl::if_(dsl::equal_sign >> dsl::p<expr>), dsl::sep(dsl::comma));
        }();

        static constexpr auto value = lexy::as_list<std::vector<ast::LambdaExpr::Arg>>;
    };

    static constexpr auto rule = kw_def >> dsl::p<ident> + dsl::p<args_> + dsl::position + dsl::curly_bracketed(dsl::p<stmt_list>) + dsl::position;
    static constexpr auto value = state_callback<ast::Expr>([](ParseState &st, std::string name, std::vector<ast::LambdaExpr::Arg> args, Pos begin, ast::ExprList body, Pos end) {
        return ast::LetExpr{name, {ast::LambdaExpr{body, args, name, st.span(begin, end)}}};
    });
};

struct stmt_list_
{
    struct expected_statement_error {
        static constexpr auto name = "expected ';'";
    };

    static constexpr const char *name() { return "statement list"; }

    static constexpr auto rule
        = dsl::peek(dsl::eof | dsl::lit_c<'}'>) >> dsl::nullopt
           | dsl::semicolon >> dsl::p<stmt_list>
           | dsl::peek(kw_if) >> (dsl::p<stmt_if> + dsl::p<stmt_list>)
           | dsl::peek(kw_def) >> (dsl::p<stmt_def> + dsl::p<stmt_list>)
           | dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> (dsl::p<stmt_let> + dsl::semicolon + dsl::p<stmt_list>)
           | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::p<stmt_call> + dsl::p<stmt_list>)
           | dsl::else_ >> (
                dsl::p<expr>
                    + (
                        dsl::peek(dsl::semicolon | dsl::eof | dsl::lit_c<'}'>)
                        | dsl::else_ >> dsl::error<expected_statement_error>)
                    + dsl::opt(dsl::peek_not(dsl::eof) >> dsl::p<stmt_list>));

    static constexpr auto value = lexy::callback<ast::ExprList>(
        [](lexy::nullopt) { return ast::ExprList{}; },
        [](ast::Expr ex, lexy::nullopt) { return ast::ExprList{std::move(ex)}; },
        [](ast::ExprList exs) { return exs; },
        [](ast::Expr ex, ast::ExprList exs) {
            auto r = ast::ExprList{std::move(ex)};
            std::move(exs.begin(), exs.end(), std::back_inserter(r));
            return r;
        }
    );
};

struct document {
    static constexpr auto whitespace = ws_rule;
    static constexpr auto rule = dsl::p<stmt_list> + dsl::eof;
    static constexpr auto value = lexy::forward<std::vector<ast::Expr>>;
};

}

struct ErrorCallback {
    Input &input;
    std::vector<LogMessage> &errors;

    using return_type = void;

    template <typename Input, typename Reader, typename Tag>
    void operator()(const lexy::error_context<Input>& context, const lexy::error<Reader, Tag>& error) {
        std::string msg;
        lexy_ext::report_error.to(std::back_inserter(msg)).sink()(context, error);

        errors.push_back(LogMessage{LogMessage::Level::Error, msg, getSpan(input, error.position(), error.position())});
    }

    constexpr auto sink() const {
        return *this;
    };
};

ParseResult parse(std::string code, bool enableSpans) {
    auto input = lexy::range_input<lexy::utf8_encoding, std::string::const_iterator>(code.cbegin(), code.cend());

    std::vector<LogMessage> errors;
    ErrorCallback errorCallback{input, errors};

    auto parseState = ParseState{input, enableSpans};

    auto result = lexy::parse<grammar::document>(input, parseState, errorCallback);

    if (result.is_error()) {
        return {{}, errors};
    }

    return {{result.value()}, errors};
}
