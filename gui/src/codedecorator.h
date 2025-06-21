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
    Q_PROPERTY(int selectionStart MEMBER m_selectionStart)
    Q_PROPERTY(int selectionEnd MEMBER m_selectionEnd)

public:
    QQuickTextDocument *textDocument() const { return m_quickTextDocument; }
    void setTextDocument(QQuickTextDocument *doc);

    QList<SpanObj> highlightedSpans() const;
    void setHighlightedSpans(QList<SpanObj> spans);

    int cursorPosition() const;
    void setCursorPosition(int position);

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result);
    Q_INVOKABLE void adjustNumber(int dir);
    Q_INVOKABLE bool handleReturn();
    Q_INVOKABLE bool handleBackspace();
    Q_INVOKABLE bool handleTab(int dir);
    Q_INVOKABLE int handleHome() const;

private:
    QQuickTextDocument *m_quickTextDocument = nullptr;
    SyntaxHighlighter *m_highlighter = nullptr;
    int m_selectionStart = -1;
    int m_selectionEnd = -1;
    int m_indentSize = 4;

    QTextBlock getBlockAt(int pos) const;
    void adjustBlockIndent(const QTextBlock &block, int dir);
};
