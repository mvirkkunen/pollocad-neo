#include <unordered_set>

#include <QFile>
#include <QGuiApplication>
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSyntaxHighlighter>

#include "occtview.h"

#include "parser.h"
#include "backgroundexecutor.h"

namespace std {
    template <> struct hash<std::pair<int, int>> {
        inline size_t operator()(const std::pair<int, int> &v) const {
            std::hash<int> int_hasher;
            return int_hasher(v.first) ^ int_hasher(v.second);
        }
    };
}

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) { }

    void setMessages(const std::vector<LogMessage> &messages) {
        std::vector<QTextBlock> blocks;
        //findBlocks(blocks);
        m_messages = messages;
        //findBlocks(blocks);

        /*for (const auto &b : blocks) {
            qDebug() << "rehighlight " << b.position();
            rehighlightBlock(b);
        }*/

        rehighlight();
    }

    void setSpanHovered(int begin, int end, bool highlight) {
        if (highlight) {
            m_hoveredSpans.insert({begin, end});
        } else {
            m_hoveredSpans.erase({begin, end});
        }

        auto it = document()->findBlock(begin);
        auto itEnd = document()->findBlock(end);

        do {
            rehighlightBlock(it);
            it = it.next();
        } while (it.length() && it != itEnd);
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

        for (const auto &hover : m_hoveredSpans) {
            int sbegin = hover.first;
            int send = hover.second;

            if (sbegin < end && sbegin > begin) {
                const int localBegin = std::max(0, sbegin - begin);
                int localEnd = std::min(static_cast<int>(text.length()), send - begin);

                while (localEnd > localBegin + 1 && localEnd >= 1 && text.at(localEnd - 1).isSpace()) {
                    localEnd--;
                }

                if (localEnd == localBegin) {
                    localEnd = std::min(end, localEnd + 1);
                }

                QTextCharFormat format;
                //format.setFontUnderline(true);
                //format.setFontOverline(true);
                //format.setFontWeight(QFont::Bold);
                format.setBackground(QColor::fromRgb(0x00, 0x80, 0x00));
                format.setForeground(QColor::fromRgb(0xff, 0xff, 0xff));
                setFormat(localBegin, localEnd - localBegin, format);
            }
        }
    }

    void findBlocks(std::vector<QTextBlock> &blocks) {
        for (const auto &msg : m_messages) {
            auto it = document()->findBlock(msg.span.begin);
            auto end = document()->findBlock(msg.span.end);

            do {
                rehighlightBlock(it);
                it = it.next();
            } while (it.length() && it != end);
        }
    }

private:
    std::vector<LogMessage> m_messages;
    std::unordered_set<std::pair<int, int>> m_hoveredSpans;
};

class CodeHighlighter: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE void setTextDocument(QQuickTextDocument *doc) {
        m_highlighter = new SyntaxHighlighter(doc->textDocument());
    }

    Q_INVOKABLE void setResult(BackgroundExecutorResult *result) {
        m_highlighter->setMessages(result->messages());
    }

    Q_INVOKABLE void setOcctView(OcctView *viewer) {
        connect(viewer, &OcctView::spanHovered, this, [this](int begin, int end, bool hovered) {
            m_highlighter->setSpanHovered(begin, end, hovered);
        });
    }

private:
    SyntaxHighlighter *m_highlighter;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/qt/qml/pollocad/res/icon.png"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    BackgroundExecutor executorManager;
    engine.rootContext()->setContextProperty("executor", &executorManager);

    CodeHighlighter highlighter;
    engine.rootContext()->setContextProperty("highlighter", &highlighter);

    QString loadedCode = "pollo();\n";
    const auto &args = QGuiApplication::arguments();
    if (args.length() >= 2) {
        const auto &path = args.at(1);
        QFile file(args.at(1));
        if (file.open(QFile::ReadOnly)) {
            loadedCode = QString::fromUtf8(file.readAll());
        } else {
            std::cerr << "Could not open " << path.toStdString() << "\n";
        }
    }
    engine.rootContext()->setContextProperty("loadedCode", loadedCode);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("pollocad", "Main");

    return app.exec();
}

#include "main.moc"
