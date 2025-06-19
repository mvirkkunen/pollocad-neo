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

using namespace ast;

constexpr auto ws_rule = dsl::ascii::space | (LEXY_LIT("/*") >> dsl::until(LEXY_LIT("*/"))) | (LEXY_LIT("//") >> dsl::until(dsl::newline).or_eof());

constexpr auto ws = dsl::whitespace(ws_rule);

constexpr auto ident_chars = dsl::identifier(dsl::ascii::alpha_underscore / dsl::dollar_sign, dsl::ascii::alpha_digit_underscore);

auto kw_if = LEXY_KEYWORD("if", ident_chars);
auto kw_else = LEXY_KEYWORD("else", ident_chars);
auto kw_def = LEXY_KEYWORD("def", ident_chars);

constexpr auto ident_class = ident_chars.reserve(kw_if, kw_else, kw_def);

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
    static constexpr auto value = lexy::forward<ExprList>;
};

struct expr : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse<struct expr_>;
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr_block {
    static constexpr auto rule = dsl::curly_bracketed(dsl::p<stmt_list>);
    static constexpr auto value = lexy::construct<LambdaExpr>;
};

struct expr_if {
    static constexpr auto name = "if statement";
    static constexpr auto rule = kw_if >> (dsl::p<expr> + dsl::p<expr_block> + dsl::if_(kw_else >> dsl::p<expr_block>));
    static constexpr auto value = lexy::callback<Expr>(
        [](Expr condition, Expr then) { return CallExpr{"if", move_vec(std::move(condition), std::move(then))}; },
        [](Expr condition, Expr then, Expr else_) { return CallExpr{"if", move_vec(std::move(condition), std::move(then), std::move(else_))}; }
    );
};

struct expr_terminal {
    static constexpr auto rule = (
        dsl::p<expr_block>
        | dsl::p<expr_if>
    );
    static constexpr auto value = lexy::forward<Expr>;
};

struct function_call_chain {
    static constexpr const char *name = "function call chain";

    struct call_ {
        static constexpr auto name = "single function call";

        struct args_ {
            static constexpr const char *name = "function call arguments";

            static constexpr auto rule = dsl::parenthesized.opt_list(dsl::opt(dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::p<ident> + dsl::equal_sign) + dsl::p<expr>, dsl::sep(dsl::comma));
            static constexpr auto value = lexy::fold_inplace<CallExpr>(
                []() { return CallExpr{}; },
                [](auto& expr, lexy::nullopt, Expr value) { expr.positional.push_back(std::move(value)); },
                [](auto& expr, std::string name, Expr value) { expr.named.insert({name, std::move(value)}); }
            ) >> lexy::callback<CallExpr>([](lexy::nullopt) { return CallExpr{}; });
        };

        static constexpr auto rule = (
            dsl::hash_sign
            | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::position(dsl::p<ident>) + dsl::position + dsl::p<args_>)
        );
        static constexpr auto value = state_callback<CallExpr>(
            [](ParseState &st) {
                return CallExpr{"prop", {LiteralExpr{"highlight"}, LiteralExpr{true}}};
            },
            [](ParseState &st, Pos begin, std::string name, Pos end, CallExpr expr) {
                return CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)};
            }
        );
    };

    static constexpr auto rule = dsl::list(dsl::p<call_>);
    static constexpr auto value = lexy::as_list<std::vector<CallExpr>>;
};

auto callExprCallback = lexy::callback<Expr>(
    [](std::vector<CallExpr> exprs, std::optional<Expr> terminal=std::nullopt) {
        Expr result = terminal ? *terminal : exprs.back();
        if (!terminal) {
            exprs.pop_back();
        }

        while (!exprs.empty()) {
            if (!std::holds_alternative<LambdaExpr>(result.inner())) {
                result = LambdaExpr{{result}};
            }

            CallExpr next = exprs.back();
            exprs.pop_back();
            next.named.emplace("$children", result);
            result = next;
        }

        return result;
    }
);

struct expr_call {
    static constexpr auto rule = dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::p<function_call_chain> + dsl::p<expr_terminal>);
    static constexpr auto value = callExprCallback;
};

struct stmt_call {
    static constexpr auto rule = dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::p<function_call_chain> + (dsl::semicolon | dsl::p<expr_terminal>));
    static constexpr auto value = callExprCallback;
};

struct expr_var {
    static constexpr const char *name = "variable reference";

    static constexpr auto rule = dsl::position(dsl::p<ident>) >> dsl::position; // what?? FIXME
    static constexpr auto value = state_callback<Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end) { return VarExpr{name, st.span(begin, end)}; }
    );
};

struct expr_list {
    struct args_ {
        static constexpr auto rule = dsl::square_bracketed.opt_list(dsl::p<expr>, dsl::ignore_trailing_sep(dsl::comma));
        static constexpr auto value = lexy::as_list<std::vector<Expr>>;
    };

    static constexpr auto rule = dsl::p<args_>;
    static constexpr auto value = lexy::callback<Expr>([](std::vector<Expr> args) { return CallExpr{"list", std::move(args)}; });
};

struct expr_number_literal : lexy::token_production {
    static constexpr auto rule = dsl::peek(dsl::sign + ws + dsl::digit<>) >> dsl::capture(dsl::token(dsl::sign + dsl::digits<> + dsl::opt(dsl::period >> dsl::digits<>)));
    static constexpr auto value = lexy::as_string<std::string>
        | lexy::callback<Expr>([](std::string value) {
            double n;
            std::istringstream(value) >> n;
            return LiteralExpr{n};
        });
};

struct expr_string_literal : lexy::token_production {
    static constexpr auto rule = dsl::quoted(-dsl::unicode::control);
    static constexpr auto value = lexy::as_string<std::string>
        >> lexy::callback<Expr>([](std::string value) { return LiteralExpr{value}; });
};

struct expr_atom {
    struct PropertyAccess {
        std::variant<Expr, std::string> index;
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
            [](ParseState &st, Pos begin, Expr index, Pos end) { return PropertyAccess{std::move(index), st.span(begin, end)}; }
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
        static constexpr auto rule = dsl::list(dsl::p<bracketed_index_op_> | dsl::p<property_access_op_>);
        static constexpr auto value = lexy::as_list<std::vector<PropertyAccess>>;
    };

    static constexpr auto rule = (dsl::parenthesized(dsl::p<expr>)
                                  | dsl::p<expr_terminal>
                                  | dsl::p<expr_call> // call has to be before var for correct match!
                                  | dsl::p<expr_var>
                                  | dsl::p<expr_list>
                                  | dsl::p<expr_number_literal>
                                  | dsl::p<expr_string_literal>
                                  | dsl::error<expected_expression_error>) + dsl::opt(dsl::p<index_ops_>);
    static constexpr auto value = lexy::callback<Expr>(
        [](Expr expr, lexy::nullopt = {}) { return std::move(expr); },
        [](Expr expr, std::vector<PropertyAccess> access) {
            // TODO: maybe this could be a fold
            for (const auto &a : access) {
                std::visit<void>(
                    [&expr, &a](const auto &v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, Expr>) {
                            expr = CallExpr{"[]", {expr, v}, {}, a.span};
                        } else if constexpr (std::is_same_v<T, std::string>) {
                            expr = CallExpr{"[]", {expr, LiteralExpr{v}}, {}, a.span};
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
        static constexpr auto op = op_minus; // / op_highlight;
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

    static constexpr auto value = lexy::callback<Expr>(
        [](ExprList exprs) { return CallExpr{"-"}; },
        [](Expr expr) { return std::move(expr); },
        [](lexy::op<op_minus>, Expr val) { return CallExpr{"-", move_vec(std::move(val))}; },
        [](Expr lhs, lexy::op<op_mul>, Expr rhs) { return CallExpr{"*", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_div>, Expr rhs) { return CallExpr{"/", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_mod>, Expr rhs) { return CallExpr{"%", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_plus>, Expr rhs) { return CallExpr{"+", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_minus>, Expr rhs) { return CallExpr{"-", move_vec(std::move(lhs), std::move(rhs))}; }
    );
};

struct stmt_def {
    struct args_ {
        static constexpr auto rule = [] {
            auto seen_named = dsl::context_flag<stmt_def>;
            // TODO ban positional after named

            return dsl::parenthesized.opt_list(dsl::p<ident> + dsl::if_(dsl::equal_sign >> dsl::p<expr>), dsl::sep(dsl::comma));
        }();

        static constexpr auto value = lexy::as_list<std::vector<LambdaExpr::Arg>>;
    };

    static constexpr auto name = "function definition";
    static constexpr auto rule = kw_def >> (dsl::p<ident> + dsl::p<args_> + dsl::position + dsl::curly_bracketed(dsl::p<stmt_list>) + dsl::position);
    static constexpr auto value = state_callback<Expr>([](ParseState &st, std::string name, std::vector<LambdaExpr::Arg> args, Pos begin, ExprList body, Pos end) {
        return LetExpr{name, {LambdaExpr{body, args, name, st.span(begin, end)}}};
    });
};

struct stmt_let {
    static constexpr auto name = "assignment statement";
    static constexpr auto rule = dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::position(dsl::p<ident>) + dsl::position + dsl::equal_sign + dsl::p<expr> + dsl::semicolon;
    static constexpr auto value = state_callback<Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end, Expr ex) { return LetExpr{std::move(name), std::move(ex), st.span(begin, end)}; }
    );
};

struct stmt_expr {
    static constexpr auto name = "expression statement";
    static constexpr auto rule = (
        dsl::semicolon // allow stray semicolons
        | dsl::p<expr_terminal>
        | dsl::p<stmt_call>
        | dsl::else_ >> dsl::p<expr> + (
            dsl::semicolon | dsl::peek(dsl::eof | dsl::lit_c<'}'>) >> dsl::nullopt
        )
    );
    static constexpr auto value = lexy::callback<Expr>(
        []() {
            return LiteralExpr{undefined};
        },
        [](Expr expr) {
            return expr;
        },
        [](Expr expr, lexy::nullopt) {
            // this would be a return position expression
            return expr;
        }
    );
};

struct stmt_list_ {
    struct expected_statement_error {
        static constexpr auto name = "expected statement";
    };

    static constexpr auto name = "statement list";

    static constexpr auto rule = dsl::opt(dsl::list(
        dsl::peek_not(dsl::eof | dsl::lit_c<'}'>) >> (
            dsl::p<stmt_def>
            | dsl::p<stmt_let>
            | dsl::else_ >> dsl::p<stmt_expr>
        )
    ));

    static constexpr auto value = lexy::as_list<std::vector<Expr>>;
};

struct document {
    static constexpr auto whitespace = ws_rule;
    static constexpr auto rule = dsl::p<stmt_list> + dsl::eof;
    static constexpr auto value = lexy::forward<std::vector<Expr>>;
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
