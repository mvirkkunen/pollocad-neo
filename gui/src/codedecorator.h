#pragma once

#include <QObject>
#include <QQuickTextDocument>
#include <qqml.h>

#include "backgroundexecutor.h"
#include "spanobj.h"

class SyntaxHighlighter;

class CodeDecorator : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument)
    Q_PROPERTY(QList<SpanObj> highlightedSpans READ highlightedSpans WRITE setHighlightedSpans)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition)

public:
    QQuickTextDocument *textDocument() const { return m_quickTextDocument; }
    void setTextDocument(QQuickTextDocument *doc);

    QList<SpanObj> highlightedSpans() const;
    void setHighlightedSpans(QList<SpanObj> spans);

    int cursorPosition() const;
    void setCursorPosition(int position);

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);

private:
    QQuickTextDocument *m_quickTextDocument = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
};
