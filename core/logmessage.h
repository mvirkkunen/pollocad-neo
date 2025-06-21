#pragma once

#include <string>
#include <iosfwd>

struct Span
{
    //Span() = default;
    //Span(const Span &) = default;

    int begin = -1;
    int end = -1;
    int line = -1;
    int column = -1;

    bool isEmpty() const { return begin == -1; }

    bool operator==(const Span &) const = default;
};

inline std::ostream &operator<<(std::ostream &os, const Span& span)  {
    if (span.begin == -1) {
        return os << "[?-?]";
    }

    return os << "[" << span.begin << "-" << span.end << "]";
}

struct LogMessage
{
    enum class Level {
        Info = 1,
        Warning = 2,
        Error = 3,
    };

    Level level;
    std::string message;
    Span span;
};
