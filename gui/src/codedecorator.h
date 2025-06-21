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

public:
    QQuickTextDocument *textDocument() const { return m_quickTextDocument; }
    void setTextDocument(QQuickTextDocument *doc);

    QList<SpanObj> highlightedSpans() const;
    void setHighlightedSpans(QList<SpanObj> spans);

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);

private:
    QQuickTextDocument *m_quickTextDocument = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
};
