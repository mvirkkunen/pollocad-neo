#include <QQuickTextDocument>
#include <QSyntaxHighlighter>
#include <QTextDocument>

#include "codedecorator.h"

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) { }

    void setMessages(const std::vector<LogMessage> &messages) {
        m_messages = messages;
        rehighlight();
    }

    QList<SpanObj> highlightedSpans() const {
        return m_highlightedSpans;
    }

    void setHighlightedSpans(QList<SpanObj> spans) {
        auto oldSpans = m_highlightedSpans;
        m_highlightedSpans = spans;
        rehighlightSpans(oldSpans);
        rehighlightSpans(m_highlightedSpans);
    }

protected:
    void highlightBlock(const QString &text) override {
        const int begin = currentBlock().position();
        const int end = begin + text.length();

        for (const auto &msg : m_messages) {
            if (msg.level == LogMessage::Level::Info) {
                continue;
            }

            if (msg.span.begin < end && msg.span.end > begin) {
                const int localBegin = std::max(0, msg.span.begin - begin);
                int localEnd = std::min(static_cast<int>(text.length()), msg.span.end - begin);

                while (localEnd > localBegin + 1 && localEnd >= 1 && text.at(localEnd - 1).isSpace()) {
                    localEnd--;
                }

                if (localEnd == localBegin) {
                    localEnd = std::min(end, localEnd + 1);
                }

                auto color = (msg.level == LogMessage::Level::Error ? QColor::fromRgb(0xff, 0x00, 0x00) : QColor::fromRgb(0x88, 0x88, 0x00));

                QTextCharFormat format;
                format.setUnderlineStyle(QTextCharFormat::UnderlineStyle::SingleUnderline);
                //format.setUnderlineStyle(QTextCharFormat::UnderlineStyle::WaveUnderline);
                format.setUnderlineColor(color);
                format.setToolTip(QString::fromStdString(msg.message));
                format.setForeground(color);
                setFormat(localBegin, localEnd - localBegin, format);
            }
        }

        for (const auto &span : m_highlightedSpans) {
            if (span.begin < end && span.begin >= begin) {
                const int localBegin = std::max(0, span.begin - begin);
                int localEnd = std::min(static_cast<int>(text.length()), span.end - begin);

                while (localEnd > localBegin + 1 && localEnd >= 1 && text.at(localEnd - 1).isSpace()) {
                    localEnd--;
                }

                if (localEnd == localBegin) {
                    localEnd = std::min(end, localEnd + 1);
                }

                QTextCharFormat format;
                format.setBackground(QColor::fromRgb(0x00, 0x80, 0x00));
                format.setForeground(QColor::fromRgb(0xff, 0xff, 0xff));
                setFormat(localBegin, localEnd - localBegin, format);
            }
        }
    }

    void rehighlightSpans(const QList<SpanObj> &spans) {
        for (const auto &span : spans) {
            auto it = document()->findBlock(span.begin);
            auto end = document()->findBlock(span.end);

            do {
                rehighlightBlock(it);
                it = it.next();
            } while (it != document()->end());
        }
    }

private:
    std::vector<LogMessage> m_messages;
    QList<SpanObj> m_highlightedSpans;
};

void CodeDecorator::setTextDocument(QQuickTextDocument *doc) {
    m_quickTextDocument = doc;
    m_highlighter = new SyntaxHighlighter(doc->textDocument());
}

QList<SpanObj> CodeDecorator::highlightedSpans() const {
    if (m_highlighter) {
        return m_highlighter->highlightedSpans();
    } else {
        return {};
    }
}

void CodeDecorator::setHighlightedSpans(QList<SpanObj> spans) {
    if (m_highlighter) {
        m_highlighter->setHighlightedSpans(spans);
    }
}

void CodeDecorator::setResult(BackgroundExecutorResult *result) {
    if (m_highlighter) {
        m_highlighter->setMessages(result->messages());
    }
}

#include "codedecorator.moc"
