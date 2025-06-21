#pragma once

#include <QObject>

#include "logmessage.h"

class SpanObj : public Span
{
    Q_GADGET

    Q_PROPERTY(int begin MEMBER begin CONSTANT)
    Q_PROPERTY(int end MEMBER end CONSTANT)

public:
    SpanObj(const SpanObj &other) : Span{other} { }
    SpanObj(const Span &other) : Span{other} { }
    SpanObj(int begin, int end) : Span{begin, end, -1, -1} { }

    bool operator==(const SpanObj &) const = default;
};
