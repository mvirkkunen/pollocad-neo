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

/*class Span
{
    Q_GADGET

    Q_PROPERTY(int begin READ begin CONSTANT);
    Q_PROPERTY(int end READ end CONSTANT);
    Q_PROPERTY(int line READ line CONSTANT);
    Q_PROPERTY(int column READ column CONSTANT);

    Q_PROPERTY(int length READ length CONSTANT);
    Q_PROPERTY(bool isEmpty READ isEmpty CONSTANT);

public:
    Span() { }

    explicit Span(int begin, int end, int line, int column)
        : m_begin(begin), m_end(end), m_line(line), m_column(column)
    { }

    int begin() const { return m_begin; }
    int end() const { return m_end; }
    int line() const { return m_line; }
    int column() const { return m_column; }

    int length() const {
        return isEmpty() ? -1 : m_end - m_begin;
    }

    bool isEmpty() const {
        return (m_begin == -1 || m_end == -1 || m_line == -1 || m_column == -1);
    }

    bool operator==(const Span& other) const = default;

private:
    int m_begin = -1;
    int m_end = -1;
    int m_line = -1;
    int m_column = -1;
};

inline std::ostream &operator<<(std::ostream &os, const Span &span) {
    if (span.isEmpty()) {
        return os << "[?-?]";
    } else {
        return os << "[" << span.begin() << "-" << span.end() << "]";
    }
}

class LogMessage
{
    Q_GADGET

    Q_PROPERTY(Level level READ level CONSTANT);
    Q_PROPERTY(QString message READ message CONSTANT);
    Q_PROPERTY(Span span READ span CONSTANT);

public:
    enum class Level {
        Info = 1,
        Warning = 2,
        Error = 3,
    };
    Q_ENUM(Level)

    LogMessage(const LogMessage &other) : m_level(other.m_level), m_message(other.m_message), m_span(other.m_span) { }

    explicit LogMessage(Level level, std::string message, Span span = Span{})
        : m_level(level), m_message(QString::fromStdString(message)), m_span(span)
    { }

    Level level() const { return m_level; }
    QString message() const { return m_message; }
    Span span() const { return m_span; }

    static LogMessage info(std::string message) { return LogMessage{Level::Info, message}; }

private:
    Level m_level;
    QString m_message;
    Span m_span;
};
*/