#pragma once

#include <iostream>
#include <iterator>
#include "lexy/dsl/base.hpp"
#include "lexy/input_location.hpp"

#include "logmessage.h"

template <typename Input, typename Iterator>
Span getSpan(const Input &input, const Iterator &start, const Iterator &end) {
    auto loc = lexy::get_input_location(input, start);
    return Span(start - input.begin(), end - input.begin(), loc.line_nr(), loc.column_nr());
}

template <typename Encoding, typename Input>
class _nice_reader
{
public:
    using encoding = Encoding;
    using iterator = Input::iterator;

    struct marker
    {
        iterator _it;

        constexpr iterator position() const noexcept
        {
            return _it;
        }
    };

    constexpr explicit _nice_reader(const Input &input) noexcept : _input(input), _cur(input.begin()), _end(input.end()) { }

    constexpr auto peek() const noexcept
    {
        if (_cur == _end)
            return encoding::eof();
        else
            return encoding::to_int_type(static_cast<typename encoding::char_type>(*_cur));
    }

    constexpr void bump() noexcept
    {
        LEXY_PRECONDITION(_cur != _end);
        ++_cur;
    }

    constexpr iterator position() const noexcept
    {
        return _cur;
    }

    constexpr marker current() const noexcept
    {
        return {_cur};
    }

    constexpr void reset(marker m) noexcept
    {
        LEXY_PRECONDITION(lexy::_detail::precedes(m._it, _end));
        _cur = m._it;
    }

    constexpr const Input &input() noexcept {
        return _input;
    }

private:
    const Input &_input;
    iterator _cur;
    iterator _end;
};

template <typename Encoding, typename Data>
class nice_input
{
public:
    using encoding  = Encoding;
    using char_type = typename encoding::char_type;

    using iterator = Data::const_iterator;
    using sentinel = iterator;

    constexpr nice_input(const Data &data) noexcept : _data(data) {}

    constexpr iterator begin() const noexcept
    {
        return std::cbegin(_data);
    }

    constexpr sentinel end() const noexcept
    {
        return std::cend(_data);
    }

    constexpr auto reader() const& noexcept
    {
        return _nice_reader<Encoding, nice_input<Encoding, Data>>(*this);
    }

private:
    const Data &_data;
};

template <typename Rule>
struct _spanr : lexyd::_copy_base<Rule>
{
    template <typename Reader>
    struct bp
    {
        typename Reader::iterator end;
        lexy::branch_parser_for<Rule, Reader> rule;

        template <typename ControlBlock>
        constexpr auto try_parse(const ControlBlock* cb, const Reader& reader)
        {
            std::cerr << "try_parse\n";
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
            std::cerr << "finish\n";
            auto begin = reader.position();
            //context.on(_ev::token{}, lexy::position_token_kind, pos, pos);
            return rule.template finish<NextParser>(context, reader, LEXY_FWD(args)..., getSpan(reader.input(), begin, end));
        }
    };

    template <typename NextParser>
    struct p
    {
        template <typename Context, typename Reader, typename... Args>
        LEXY_PARSER_FUNC static bool parse(Context& context, Reader& reader, Args&&... args)
        {
            std::cerr << "parse\n";
            using Parser = lexy::parser_for<Rule, NextParser>;
            auto begin = reader.position();
            //context.on(_ev::token{}, lexy::position_token_kind, pos, pos);
            if (!Parser::parse(context, reader, LEXY_FWD(args)..., getSpan(reader.input(), begin, begin))) {
                return false;
            }

            auto end = reader.position();
            return Parser::parse(context, reader, LEXY_FWD(args)..., getSpan(reader.input(), begin, end));
        }
    };
};

/*struct _span_dsl
{
    template <typename Rule>
    constexpr auto operator()(Rule) const
    {
        return _spanr<Rule>{};
    }
};*/

template <typename Rule>
constexpr auto span(Rule) {
    return _spanr<Rule>{};
}

//} // namespace lexyd
