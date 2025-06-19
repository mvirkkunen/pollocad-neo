#include <QGuiApplication>
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSyntaxHighlighter>

#include "parser.h"
#include "backgroundexecutor.h"

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
