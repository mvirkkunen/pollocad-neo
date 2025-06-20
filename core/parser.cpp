#include <format>
#include <sstream>

#include "lexy/action/parse.hpp"
#include "lexy/callback.hpp"
#include "lexy/dsl.hpp"
#include "lexy_ext/report_error.hpp"

#include "parser.h"

using Input = lexy::range_input<lexy::utf8_encoding, std::string::const_iterator>;

using Pos = Input::iterator;

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

namespace grammar
{

namespace dsl = lexy::dsl;

using namespace ast;

constexpr auto ws_rule = dsl::ascii::space | (LEXY_LIT("/*") >> dsl::until(LEXY_LIT("*/"))) | (LEXY_LIT("//") >> dsl::until(dsl::newline).or_eof());

constexpr auto ws = dsl::whitespace(ws_rule);

constexpr auto ident_chars = dsl::identifier(dsl::ascii::alpha_underscore / dsl::dollar_sign, dsl::ascii::alpha_digit_underscore);

constexpr auto kw_if = LEXY_KEYWORD("if", ident_chars);
constexpr auto kw_else = LEXY_KEYWORD("else", ident_chars);
constexpr auto kw_for = LEXY_KEYWORD("for", ident_chars);
constexpr auto kw_def = LEXY_KEYWORD("def", ident_chars);

constexpr auto ident_class = ident_chars.reserve(kw_if, kw_else, kw_for, kw_def);

constexpr auto end_of_block = dsl::eof | dsl::lit_c<'}'>;

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

struct stmt_expr : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse_branch<struct stmt_expr_>;
    static constexpr auto value = lexy::forward<Expr>;
};

struct stmt_expr_as_lambda : lexy::transparent_production {
    static constexpr auto rule = dsl::p<stmt_expr>;
    static constexpr auto value = lexy::callback<Expr>([](Expr expr) { return LambdaExpr{ExprPtr{expr}}; });
};

struct stmt_list : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse<struct stmt_list_>;
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse<struct expr_>;
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr_terminal : lexy::transparent_production {
    static constexpr auto rule = dsl::recurse_branch<struct expr_terminal_>;
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr_block {
    static constexpr auto name = "block expression";
    static constexpr auto rule = dsl::curly_bracketed(dsl::p<stmt_list>);
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr_if {
    static constexpr auto name = "if statement";
    static constexpr auto rule = kw_if >> (
        dsl::parenthesized(dsl::p<expr>)
        + dsl::p<stmt_expr_as_lambda>
        + dsl::if_(kw_else >> dsl::p<stmt_expr_as_lambda>)
    );
    static constexpr auto value = lexy::as_list<std::vector<Expr>> | lexy::callback<Expr>(
        [](std::vector<Expr> exprs) { return CallExpr{"if", exprs}; }
    );
};

struct expr_terminal_ {
    static constexpr const char *name = "terminal expression";
    static constexpr auto rule = (
        dsl::p<expr_block>
        | dsl::p<expr_if>
    );
    static constexpr auto value = lexy::forward<Expr>;
};

struct expr_call {
    struct args_ {
        static constexpr const char *name = "function call arguments";

        static constexpr auto rule = dsl::parenthesized.opt_list(dsl::opt(dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::p<ident> + dsl::equal_sign) + dsl::p<expr>, dsl::sep(dsl::comma));
        static constexpr auto value = lexy::fold_inplace<CallExpr>(
            []() { return CallExpr{}; },
            [](auto& expr, lexy::nullopt, Expr value) { expr.positional.push_back(std::move(value)); },
            [](auto& expr, std::string name, Expr value) { expr.named.insert({name, std::move(value)}); }
        ) >> lexy::callback<CallExpr>([](lexy::nullopt) { return CallExpr{}; });
    };
    static constexpr auto rule = dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>) >> (dsl::position(dsl::p<ident>) + dsl::position + dsl::p<args_>);
    static constexpr auto value = lexy::callback_with_state<CallExpr>(
        [](ParseState &st, Pos begin, std::string name, Pos end, CallExpr expr) {
            return CallExpr{name, std::move(expr.positional), std::move(expr.named), st.span(begin, end)};
        }
    );
};

struct function_call_chain {
    static constexpr const char *name = "function call chain";

    struct call_highlight_ {
        static constexpr auto rule = dsl::hash_sign;
        static constexpr auto value = lexy::callback<CallExpr>(
            []() { return CallExpr{"prop", {LiteralExpr{"highlight"}, LiteralExpr{true}}}; }
        );
    };

    struct call_for_ {
        static constexpr const char *name = "for expression";

        struct list_like_iterable_ {
            static constexpr auto name = "list-like for loop iterable";

            struct rest_of_list_items_ {
                static constexpr auto name = "list literal";
                static constexpr auto rule = dsl::list(dsl::p<expr>, dsl::sep(dsl::comma));
                static constexpr auto value = lexy::as_list<std::vector<Expr>>;
            };

            static constexpr auto rule = dsl::square_bracketed.opt( // opt for [] case
                dsl::p<expr> + dsl::opt( // opt for [a] case
                    dsl::colon >> (dsl::p<expr> + dsl::opt(dsl::colon >> dsl::p<expr>)) // [from:to] or [from:step:to] case
                    | dsl::comma >> dsl::p<rest_of_list_items_> // [a, b, c] case (a is captured already above)
                )
            );
            static constexpr auto value = lexy::callback<std::vector<Expr>>(
                // [] case (could probably be replaced with just "undefined" as iterating over nothing does nothing)
                [](lexy::nullopt={}) {
                    return std::vector<Expr>{Expr{CallExpr{"list"}}};
                },
                // [from:to] or [from:step:to] case
                [](Expr from, Expr toOrStep, std::optional<Expr> toIfStep={}) {
                    Expr step = toIfStep.has_value() ? std::move(toOrStep) : Expr{LiteralExpr{1.0}};
                    Expr to = toIfStep.has_value() ? std::move(*toIfStep) : std::move(toOrStep);
                    return std::vector<Expr>{std::move(from), std::move(step), std::move(to)};
                },
                // [a] case
                [](Expr item, lexy::nullopt) {
                    return std::vector<Expr>{Expr{CallExpr{"list", {std::move(item)}}}};
                },
                // [a, b, ...] case
                [](Expr first, std::vector<Expr> rest) {
                    rest.insert(rest.begin(), std::move(first));
                    return std::vector<Expr>{Expr{CallExpr{"list", std::move(rest)}}};
                }
            );
        };

        // OOF why did I have to choose to support the same syntax as OpenSCAD
        // the iterable be either: [from:to], [from:step:to] _or_ a list literal [a, b, c] _or_ an empty list [] literal _or_ a general expression
        static constexpr auto rule = kw_for >> dsl::parenthesized(
            dsl::p<ident> // variable name
            + dsl::equal_sign
            + (dsl::p<list_like_iterable_>
                | dsl::else_ >> dsl::p<expr>) // general expression case
        );
        static constexpr auto value = lexy::callback<CallExpr>(
            // general expression case
            [](std::string name, Expr expr) {
                return CallExpr{"for", {expr, LiteralExpr{name}}};
            },
            // list-like case
            [](std::string name, std::vector<Expr> args) {
                args.push_back(LiteralExpr{name});
                return CallExpr{"for", args};
            }
        );
    };

    static constexpr auto rule = dsl::list(
        dsl::p<call_highlight_>
        | dsl::p<call_for_>
        | dsl::p<expr_call>
    );
    static constexpr auto value = lexy::as_list<std::vector<CallExpr>>;
};

struct stmt_call {
    static constexpr auto name = "function call statement";
    static constexpr auto rule = (dsl::peek(dsl::hash_sign | kw_for) | dsl::peek(dsl::p<ident> + ws + dsl::lit_c<'('>)) >> (
        dsl::p<function_call_chain> + (
            dsl::peek(end_of_block)
            | dsl::semicolon
            | dsl::p<expr_terminal>
            | dsl::else_ >> (dsl::p<expr> + dsl::semicolon))
    );
    static constexpr auto value = lexy::callback<Expr>(
        [](std::vector<CallExpr> exprs, std::optional<Expr> terminal=std::nullopt) {
            Expr result = terminal ? *terminal : exprs.back();
            if (!terminal) {
                exprs.pop_back();
            }

            while (!exprs.empty()) {
                // TODO this is a bit iffy
                if (!std::holds_alternative<LambdaExpr>(result.inner())) {
                    result = LambdaExpr{{result}};
                }

                CallExpr next = exprs.back();
                exprs.pop_back();

                // maybe get rid of this hack someday
                if (next.func == "for") {
                    auto nameExpr = next.positional.back();
                    if (auto pLiteralExpr = std::get_if<LiteralExpr>(&nameExpr.inner())) {
                        if (auto pname = pLiteralExpr->value->as<std::string>()) {
                            if (auto plambda = std::get_if<LambdaExpr>(&result.inner())) {
                                next.positional.pop_back();
                                plambda->args.push_back(LambdaExpr::Arg{*pname});
                            }
                        }
                    }
                }

                next.named.emplace("$children", result);
                result = next;
            }

            return result;
        }
    );
};

struct expr_var {
    static constexpr auto name = "variable reference";
    static constexpr auto rule = dsl::position(dsl::p<ident>) >> dsl::position;
    static constexpr auto value = lexy::callback_with_state<Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end) { return VarExpr{name, st.span(begin, end)}; }
    );
};

struct expr_list_literal {
    struct args_ {
        static constexpr auto name = "list literal items";
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
    struct quoted_string_ {
        static constexpr auto name = "quoted string";
        static constexpr auto rule = dsl::quoted(-dsl::unicode::control);
        static constexpr auto value = lexy::as_string<std::string>;
    };

    struct symbol_string_ {
        static constexpr auto name = "symbol string";
        static constexpr auto rule = dsl::colon >> dsl::capture(dsl::token(ident_chars));
        static constexpr auto value = lexy::as_string<std::string>;
    };

    static constexpr auto rule = dsl::p<quoted_string_> | dsl::p<symbol_string_>;
    static constexpr auto value = lexy::as_string<std::string>
        >> lexy::callback<Expr>([](std::string value) { return LiteralExpr{value}; });
};

struct expr_atom {
    static constexpr auto name = "expression atom";

    struct PropertyAccess {
        std::variant<Expr, std::string> index;
        Span span;
    };

    struct expected_expression_error {
        static constexpr auto name = "expected expression";
    };

    struct bracketed_index_op_ : lexy::transparent_production {
        static constexpr auto name = "indexing expression";
        static constexpr auto rule = dsl::square_bracketed(dsl::position + dsl::p<expr> + dsl::position);
        static constexpr auto value = lexy::callback_with_state<PropertyAccess>(
            [](ParseState &st, Pos begin, Expr index, Pos end) { return PropertyAccess{std::move(index), st.span(begin, end)}; }
        );
    };

    struct property_access_op_ : lexy::transparent_production {
        static constexpr auto name = "property access expression";
        static constexpr auto rule = dsl::lit_c<'.'> >> (dsl::position + dsl::p<ident> + dsl::position);
        static constexpr auto value = lexy::callback_with_state<PropertyAccess>(
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
                                  | dsl::p<expr_list_literal>
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
auto op_c = dsl::op(dsl::lit_c<c>);

#define OP_S(NAME) dsl::op(LEXY_LIT(NAME));

constexpr auto op_plus = op_c<'+'>;
constexpr auto op_minus = op_c<'-'>;
constexpr auto op_not = op_c<'!'>;
constexpr auto op_bnot = op_c<'~'>;

constexpr auto op_mul = op_c<'*'>;
constexpr auto op_div = op_c<'/'>;
constexpr auto op_mod = op_c<'%'>;

constexpr auto op_lt = OP_S("<");
constexpr auto op_le = OP_S("<=");
constexpr auto op_gt = OP_S(">");
constexpr auto op_ge = OP_S(">=");

constexpr auto op_eq = OP_S("==");
constexpr auto op_ne = OP_S("!=");

constexpr auto op_band = op_c<'&'>;

constexpr auto op_bor = op_c<'|'>;

struct expr_ : lexy::expression_production
{
    static constexpr auto name = "expression";
    static constexpr auto atom = dsl::p<expr_atom>;

    struct minus_not_bnot_ : dsl::prefix_op
    {
        static constexpr auto op = op_plus / op_minus / op_not / op_bnot;
        using operand = dsl::atom;
    };

    struct mul_div_mod_ : dsl::infix_op_left
    {
        static constexpr auto op = op_mul / op_div / op_mod;
        using operand = minus_not_bnot_;
    };

    struct plus_minus_ : dsl::infix_op_left
    {
        static constexpr auto op = op_plus / op_minus;
        using operand = mul_div_mod_;
    };

    struct lt_le_gt_ge_ : dsl::infix_op_left
    {
        static constexpr auto op = op_lt / op_le / op_gt / op_ge;
        using operand = plus_minus_;
    };

    struct eq_ne_ : dsl::infix_op_left
    {
        static constexpr auto op = op_eq / op_ne;
        using operand = lt_le_gt_ge_;
    };

    struct band_ : dsl::infix_op_left
    {
        static constexpr auto op = op_band;
        using operand = eq_ne_;
    };

    struct bor_ : dsl::infix_op_left
    {
        static constexpr auto op = op_bor;
        using operand = band_;
    };

    using operation = bor_;

    static constexpr auto value = lexy::callback<Expr>(
        [](Expr expr) { return std::move(expr); },
        [](lexy::op<op_plus>, Expr val) { return CallExpr{"+", move_vec(Expr{LiteralExpr{0.0}}, std::move(val))}; },
        [](lexy::op<op_minus>, Expr val) { return CallExpr{"-", move_vec(Expr{LiteralExpr{0.0}}, std::move(val))}; },
        [](lexy::op<op_not>, Expr val) { return CallExpr{"!", move_vec(std::move(val))}; },
        [](lexy::op<op_bnot>, Expr val) { return CallExpr{"~", move_vec(std::move(val))}; },

        [](Expr lhs, lexy::op<op_mul>, Expr rhs) { return CallExpr{"*", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_div>, Expr rhs) { return CallExpr{"/", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_mod>, Expr rhs) { return CallExpr{"%", move_vec(std::move(lhs), std::move(rhs))}; },

        [](Expr lhs, lexy::op<op_plus>, Expr rhs) { return CallExpr{"+", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_minus>, Expr rhs) { return CallExpr{"-", move_vec(std::move(lhs), std::move(rhs))}; },

        [](Expr lhs, lexy::op<op_lt>, Expr rhs) { return CallExpr{"<", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_le>, Expr rhs) { return CallExpr{"<=", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_gt>, Expr rhs) { return CallExpr{">", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_ge>, Expr rhs) { return CallExpr{">=", move_vec(std::move(lhs), std::move(rhs))}; },

        [](Expr lhs, lexy::op<op_eq>, Expr rhs) { return CallExpr{"==", move_vec(std::move(lhs), std::move(rhs))}; },
        [](Expr lhs, lexy::op<op_ne>, Expr rhs) { return CallExpr{"!=", move_vec(std::move(lhs), std::move(rhs))}; },

        [](Expr lhs, lexy::op<op_band>, Expr rhs) { return CallExpr{"&", move_vec(std::move(lhs), std::move(rhs))}; },

        [](Expr lhs, lexy::op<op_bor>, Expr rhs) { return CallExpr{"|", move_vec(std::move(lhs), std::move(rhs))}; }
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
    static constexpr auto value = lexy::callback_with_state<Expr>([](ParseState &st, std::string name, std::vector<LambdaExpr::Arg> args, Pos begin, Expr body, Pos end) {
        return LetExpr{name, {LambdaExpr{ExprPtr{body}, args, name, st.span(begin, end)}}};
    });
};

struct stmt_let {
    static constexpr auto name = "assignment statement";
    static constexpr auto rule = dsl::peek(dsl::p<ident> + ws + dsl::equal_sign) >> dsl::position(dsl::p<ident>) + dsl::position + dsl::equal_sign + dsl::p<expr> + dsl::semicolon;
    static constexpr auto value = lexy::callback_with_state<Expr>(
        [](ParseState &st, Pos begin, std::string name, Pos end, Expr ex) { return LetExpr{std::move(name), std::move(ex), st.span(begin, end)}; }
    );
};

struct stmt_expr_ {
    struct expected_expression_error {
        static constexpr auto name = "expected expression";
    };

    static constexpr auto name = "expression statement";
    static constexpr auto rule = (
        dsl::semicolon // allow stray semicolons
        | dsl::p<expr_terminal>
        | dsl::p<stmt_call>
        | dsl::else_ >> dsl::p<expr> + (
            dsl::semicolon
            | dsl::peek(end_of_block) >> dsl::nullopt
            | dsl::else_ >> dsl::error<expected_expression_error>
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
    struct stmt_exprs_ {
        static constexpr auto name = "statement list";

        static constexpr auto rule = dsl::opt(dsl::list(
            dsl::peek_not(end_of_block) >> (
                dsl::p<stmt_def>
                | dsl::p<stmt_let>
                | dsl::else_ >> dsl::p<stmt_expr>
            )
        ));
        static constexpr auto value = lexy::as_list<std::vector<Expr>>;
    };

    static constexpr auto rule = dsl::p<stmt_exprs_>;
    static constexpr auto value = lexy::callback<Expr>([](std::vector<Expr> exprs) { return BlockExpr{exprs}; });
};

struct document {
    static constexpr auto whitespace = ws_rule;
    static constexpr auto rule = dsl::p<stmt_list> + dsl::eof;
    static constexpr auto value = lexy::forward<Expr>;
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
