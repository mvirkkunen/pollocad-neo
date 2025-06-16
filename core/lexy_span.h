#pragma once

#include <lexy/dsl/base.hpp>

template <typename Position>
struct span
{
    Position begin;
    Position end;
};

//namespace lexyd
//{

template <typename Rule>
struct _spanr : lexyd::_copy_base<Rule>
{
    template <typename Reader>
    struct bp
    {
        typename Reader::marker end;
        lexy::branch_parser_for<Rule, Reader> rule;

        template <typename ControlBlock>
        constexpr auto try_parse(const ControlBlock* cb, const Reader& reader)
        {
            auto result = rule.try_parse(cb, reader);
            end = reader.position();
            return result;
        }

        template <typename Context>
        constexpr void cancel(Context& context)
        {
            rule.cancel(context);
        }

        template <typename NextParser, typename Context, typename... Args>
        LEXY_PARSER_FUNC auto finish(Context& context, Reader& reader, Args&&... args)
        {
            auto begin = reader.position();
            //context.on(_ev::token{}, lexy::position_token_kind, pos, pos);
            return rule.template finish<NextParser>(context, reader, LEXY_FWD(args)..., span{begin, end});
        }
    };

    template <typename NextParser>
    struct p
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_PARSER_FUNC static bool parse(Context& context, Reader& reader, Args&&... args)
        {
            using Parser = lexy::parser_for<Rule, NextParser>;
            auto pos = reader.position();
            //context.on(_ev::token{}, lexy::position_token_kind, pos, pos);
            return Parser::parse(context, reader, LEXY_FWD(args)..., span{pos, pos});
        }
    };
};

struct _span_dsl
{
    template <typename Rule>
    constexpr auto operator()(Rule) const
    {
        return _spanr<Rule>{};
    }
};

constexpr auto span_of = _span_dsl{};
//} // namespace lexyd
