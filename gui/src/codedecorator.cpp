#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextDocument>

#include "codedecorator.h"

namespace
{

int leadingSpaceCount(QString str) {
    int spaces = 0;
    for (; spaces < str.length() && str.at(spaces).isSpace(); spaces++) { }
    return spaces;
}

}

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

    struct BracketInfo {
        int position = -1;
        int level = -1;
        int direction = -1;
    };

    struct BlockData : public QTextBlockUserData {
        QList<BracketInfo> brackets;

        BlockData(QList<BracketInfo> brackets) : brackets(brackets) { }
    };

public:
    SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {
        for (int i = 0; i <= c_maxIndex; i++) {
            QTextCharFormat format;
            initFormat(i, format);
            m_formats.push_back(format);
        }
    }

    static void initFormat(int index, QTextCharFormat &f) {
        switch (index) {
            case c_keyword:
                f.setForeground(QColorConstants::DarkGreen);
                break;
            case c_ident:
                f.setForeground(QColorConstants::DarkBlue);
                break;
            case c_number:
            case c_string:
            case c_symbolString:
                f.setForeground(QColorConstants::DarkRed);
                break;
            case c_beginBlockComment:
            case c_endBlockComment:
            case c_lineComment:
                f.setForeground(QColorConstants::Gray);
                break;
            case c_firstBracketFormat:
                f.setForeground(QColorConstants::DarkYellow);
                break;
            case c_firstBracketFormat + 1:
                f.setForeground(QColorConstants::DarkMagenta);
                break;
            case c_firstBracketFormat + 2:
                f.setForeground(QColorConstants::DarkBlue);
                break;
            default:
                break;
        }
    }

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

    int cursorPosition() const {
        return m_cursorPosition;
    }

    void setCursorPosition(int position) {
        if (m_cursorPosition == position) {
            return;
        }

        int oldCursor = m_cursorBracketPosition;
        int oldMatching = m_matchingBracketPosition;

        m_cursorPosition = position;

        updateBracketPositions();

        if (oldCursor != -1) {
            rehighlightBlock(document()->findBlock(oldCursor));
            rehighlightBlock(document()->findBlock(oldMatching));
        }

        if (m_cursorBracketPosition != -1) {
            rehighlightBlock(document()->findBlock(m_cursorBracketPosition));
            rehighlightBlock(document()->findBlock(m_matchingBracketPosition));
        }
    }

    void updateBracketPositions() {
        m_cursorBracketPosition = -1;
        m_matchingBracketPosition = -1;

        auto block = document()->findBlock(m_cursorPosition);
        BlockData const *data = reinterpret_cast<BlockData *>(block.userData());
        if (!data) {
            return;
        }
        QList<BracketInfo> const *brackets = &data->brackets;

        BracketInfo info;
        int index = brackets->length() - 1;
        for (; index >= 0; index--) {
            if ((*brackets)[index].position == m_cursorPosition || (*brackets)[index].position == m_cursorPosition - 1) {
                info = (*brackets)[index];
                break;
            }
        }

        if (info.position == -1) {
            return;
        }

        index += info.direction;

        do {
            if (brackets) {
                const int endIndex = (info.direction == 1) ? brackets->size() : -1;
                for (; index != endIndex; index += info.direction) {
                    if ((*brackets)[index].level == info.level) {
                        m_cursorBracketPosition = info.position;
                        m_matchingBracketPosition = (*brackets)[index].position;
                        return;
                    }
                }
            }

            block = (info.direction == 1) ? block.next() : block.previous();

            data = reinterpret_cast<BlockData *>(block.userData());
            brackets = data ? &data->brackets : nullptr;

            if (brackets) {
                index = (info.direction == 1) ? 0 : brackets->size() - 1;
            }
        } while (block.isValid());
    }

protected:
    void highlightBlock(const QString &text) override {
        highlightCode(text);
        highlightMessages();
        highlightSpans();
    }

    void highlightCode(const QString &text) {
        int state = ~previousBlockState();
        int bracketLevel = state & c_bracketLevelMask;
        bool inComment = state & c_inComment;

        QList<BracketInfo> brackets;

        auto it = m_re.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            int position = match.capturedStart() + currentBlock().position();
            int index = match.lastCapturedIndex();

            switch (index) {
                case c_beginBracket:
                    if (!inComment) {
                        bracketLevel++;
                        brackets.emplaceBack(position, bracketLevel, 1);
                    }
                    break;
                case c_beginBlockComment:
                    inComment = true;
                    break;
            }

            int formatIndex = index;

            if (inComment)  {
                formatIndex = c_beginBlockComment;
            } else if (index == c_beginBracket || index == c_endBracket) {
                formatIndex = c_firstBracketFormat + (bracketLevel % c_bracketFormatCount);
            }

            if (formatIndex < m_formats.size()) {
                auto format = m_formats[formatIndex];

                if (position == m_cursorBracketPosition || position == m_matchingBracketPosition) {
                    format.setBackground(QColor::fromRgb(0x80, 0xff, 0x80));
                }

                if (position == m_matchingBracketPosition) {
                    format.setFontWeight(QFont::Bold);
                }

                setFormat(match.capturedStart(), match.capturedLength(), format);
            }

            switch (index) {
                case c_endBracket:
                    if (!inComment) {
                        brackets.emplaceBack(position, bracketLevel, -1);
                        bracketLevel--;
                    }
                    break;
                case c_endBlockComment:
                    inComment = false;
                    break;
            }
        }

        state = (inComment ? c_inComment : 0) | (bracketLevel & c_bracketLevelMask);
        setCurrentBlockState(~state);

        if (!brackets.isEmpty()) {
            setCurrentBlockUserData(new BlockData{brackets});
        }
    }

    void highlightMessages() {
        for (const auto &msg : m_messages) {
            if (msg.level == LogMessage::Level::Info) {
                continue;
            }

            if (currentBlockContainsSpan(msg.span)) {
                auto color = (msg.level == LogMessage::Level::Error ? QColor::fromRgb(0xff, 0x00, 0x00) : QColor::fromRgb(0x88, 0x88, 0x00));

                QTextCharFormat format;
                format.setUnderlineStyle(QTextCharFormat::UnderlineStyle::SingleUnderline);
                format.setUnderlineColor(color);
                format.setToolTip(QString::fromStdString(msg.message));
                format.setForeground(color);
                setSpanFormat(msg.span, format);
            }
        }
    }

    void highlightSpans() {
        for (const auto &span : m_highlightedSpans) {
            if (currentBlockContainsSpan(span)) {
                QTextCharFormat format;
                format.setBackground(QColor::fromRgb(0x00, 0x80, 0x00));
                format.setForeground(QColor::fromRgb(0xff, 0xff, 0xff));
                setSpanFormat(span, format);
            }
        }
    }

    bool currentBlockContainsSpan(const Span &span) {
        auto position = currentBlock().position();
        return (span.begin < position + currentBlock().length() && span.end >= position);
    }

    void setSpanFormat(const Span &span, QTextCharFormat format) {
        const auto text = currentBlock().text();
        const auto position = currentBlock().position();
        const auto length = static_cast<int>(text.length());

        const int localBegin = std::max(0, span.begin - position);
        int localEnd = std::min(length, span.end - position);

        while (localEnd > localBegin + 1 && localEnd >= 1 && text.at(localEnd - 1).isSpace()) {
            localEnd--;
        }

        if (localEnd == localBegin) {
            localEnd = std::min(length, localEnd + 1);
        }

        setFormat(localBegin, localEnd - localBegin, format);
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
    static constexpr const int c_keyword = 1;
    static constexpr const int c_ident = 2;
    static constexpr const int c_number = 3;
    static constexpr const int c_beginBracket = 4;
    static constexpr const int c_endBracket = 5;
    static constexpr const int c_string = 6;
    static constexpr const int c_symbolString = 7;
    static constexpr const int c_lineComment = 8;
    static constexpr const int c_beginBlockComment = 9;
    static constexpr const int c_endBlockComment = 10;
    static constexpr const int c_firstBracketFormat = 11;
    static constexpr const int c_maxIndex = 13;
    static constexpr const int c_bracketFormatCount = 3;

    static constexpr const int c_bracketLevelMask = 0xffff;
    static constexpr const int c_inComment = 0x10000;

    QRegularExpression m_re{
        "(if|else|for|def)"
        "|([$_a-z][_a-z0-9]*)"
        "|([+-]?[0-9]+(?:\\.[0-9]+)?)"
        "|([({\\[])"
        "|([)}\\]])"
        "|(\".*?(?<!\\\\)\")"
        "|(:[$_a-z][_a-z0-9]*)"
        "|(//.*)"
        "|(/\\*.*)"
        "|(.*?\\*/)"
        "|.?"
    };

    std::vector<QTextCharFormat> m_formats;
    std::vector<LogMessage> m_messages;
    QList<SpanObj> m_highlightedSpans;
    int m_cursorPosition = -1;
    int m_cursorBracketPosition = -1;
    int m_matchingBracketPosition = -1;
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

int CodeDecorator::cursorPosition() const {
    if (m_highlighter) {
        return m_highlighter->cursorPosition();
    } else {
        return -1;
    }
}

void CodeDecorator::setCursorPosition(int position) {
    if (m_highlighter) {
        m_highlighter->setCursorPosition(position);
    }
}

void CodeDecorator::setResult(BackgroundExecutorResult *result) {
    if (m_highlighter) {
        m_highlighter->setMessages(result->messages());
    }
}

void CodeDecorator::adjustNumber(int dir) {
    auto block = getBlockAt(cursorPosition());

    const auto text = block.text();
    const int cursor = std::max(cursorPosition(), 1);

    int start = cursor - 1;
    for (; start > 0; start--) {
        QChar ch = text.at(start);
        if (!(ch.isDigit() || ch == '.' || ch == '-')) {
            break;
        }
    }

    if (start > 0) {
        start++;
    }

    if (start == cursor) {
        return;
    }

    auto num = text.sliced(start, cursor - start);

    int dot = num.indexOf(".");
    if (dot != -1) {
        num = num.removeAt(dot);
        dot = num.length() - dot;
    }

    int intNum = num.toInt() + dir;
    int idx = (intNum < 0) ? 1 : 0;
    num = QString::number(intNum);

    if (dot != -1) {
        while (num.length() < dot + idx) {
            num = num.sliced(0, idx) + "0" + num.sliced(idx);
        }

        num = num.insert(num.length() - dot, ".");
    }

    QTextCursor textCursor(block);
    textCursor.setPosition(start);
    textCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, cursor);
    textCursor.insertText(num);
}

bool CodeDecorator::handleReturn() {
    auto block = getBlockAt(m_selectionStart);

    const auto text = block.text();
    if (text.isEmpty()) {
        return false;
    }

    QString toInsert = "\n";

    int spaces = leadingSpaceCount(text);
    toInsert += text.sliced(0, spaces);

    QChar last = text.at(text.length() - 1);
    
    int cursor = cursorPosition();
    if (cursor == block.position() + text.length() && (last == '{' || last == '(' || last == '[')) {
        toInsert += QString{' '}.repeated(m_indentSize);
    }

    QTextCursor textCursor(block);
    textCursor.setPosition(m_selectionStart);
    textCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m_selectionEnd - m_selectionStart);
    textCursor.insertText(toInsert);

    return true;
}

bool CodeDecorator::handleBackspace() {
    auto block = getBlockAt(m_selectionStart);
    if (m_selectionEnd != m_selectionStart) {
        return false;
    }

    QTextCursor textCursor(block);
    textCursor.setPosition(m_selectionStart);
    textCursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);

    if (textCursor.selectedText().isEmpty() || !textCursor.selectedText().trimmed().isEmpty()) {
        return false;
    }

    textCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, (textCursor.selectedText().length() - 1) / m_indentSize * m_indentSize);
    textCursor.removeSelectedText();

    return true;
}

bool CodeDecorator::handleTab(int dir) {
    auto block = getBlockAt(m_selectionStart);
    auto endBlock = getBlockAt(m_selectionEnd);

    if (block == endBlock && dir > 0) {
        QTextCursor textCursor(block);
        textCursor.setPosition(m_selectionStart);
        textCursor.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);

        if (!textCursor.selectedText().trimmed().isEmpty()) {
            textCursor.setPosition(m_selectionStart);
            textCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, m_selectionEnd - m_selectionStart);
            textCursor.insertText(QString{' '}.repeated(m_indentSize));

            return true;
        }
    }

    while (true) {
        adjustBlockIndent(block, dir);
        if (block == endBlock) {
            break;
        }
        block = block.next();
    }

    return true;
}

int CodeDecorator::handleHome() const {
    auto block = getBlockAt(cursorPosition());
    int cursor = cursorPosition() - block.position();
    int spaces = leadingSpaceCount(block.text());

    return block.position() + ((cursor == spaces) ? 0 : spaces);
}

QTextBlock CodeDecorator::getBlockAt(int pos) const {
    return m_quickTextDocument ? m_quickTextDocument->textDocument()->findBlock(pos) : QTextBlock{};
}

void CodeDecorator::adjustBlockIndent(const QTextBlock &block, int dir) {
    const auto text = block.text();
    int spaces = leadingSpaceCount(text);

    int newSpaces = std::max(0, ((dir < 0) ? (spaces - 1) : (spaces + m_indentSize)) / m_indentSize * m_indentSize);

    QTextCursor textCursor(block);
    textCursor.movePosition(QTextCursor::StartOfLine);

    if (newSpaces < spaces) {
        textCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, spaces - newSpaces);
        textCursor.removeSelectedText();
    } else {
        textCursor.insertText(QString{' '}.repeated(newSpaces - spaces));
    }
}

#include "codedecorator.moc"
