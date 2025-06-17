#include <QGuiApplication>
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSyntaxHighlighter>

#include "parser.h"
#include "executor.h"

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
        //qDebug() << "highlight: " + text;

        const int begin = currentBlock().position();
        const int end = begin + currentBlock().length();

        //qDebug() << "global: " << begin << "-" << end;

        //QTextCharFormat format;
        //format.setForeground(QColor::fromRgb(255, 255, 0));
        //setFormat(0, 10, QColor::fromRgb(255, 255, 0));

        for (const auto &msg : m_messages) {
            //qDebug() << "checking: " << msg.span().begin() << "-" << msg.span().end();

            if (msg.span().begin() < end && msg.span().end() > begin) {
                int localBegin = std::max(0, msg.span().begin() - begin);
                int localEnd = std::min(end, msg.span().end() - begin);

                while (localEnd > localBegin + 1 && text.at(localEnd - 1).isSpace()) {
                    localEnd--;
                }

                if (localEnd == localBegin) {
                    localEnd = std::min(end, localEnd + 1);
                }

                //qDebug() << "found span: " << localBegin << "-" << localEnd << " " << msg.message();

                QTextCharFormat format;
                //format.setFontUnderline(true);
                format.setUnderlineStyle(QTextCharFormat::UnderlineStyle::SingleUnderline);
                //format.setUnderlineStyle(QTextCharFormat::UnderlineStyle::WaveUnderline);
                format.setUnderlineColor(QColor::fromRgb(255, 0, 0));
                format.setToolTip(msg.message());
                format.setForeground(QColor::fromRgb(255, 0, 0));
                setFormat(localBegin, localEnd - localBegin, format);
            }
        }
    }

    void findBlocks(std::vector<QTextBlock> &blocks) {
        for (const auto &msg : m_messages) {

            auto it = document()->findBlock(msg.span().begin());
            auto end = document()->findBlock(msg.span().end());

            qDebug() << "finding " << msg.span().begin() << "-" << msg.span().end() << " " << it.length();

            do {
                qDebug() << "found: " << it.position();
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

    Q_INVOKABLE void setResult(ExecutorResult *result) {
        m_highlighter->setMessages(result->messages());
    }

private:
    SyntaxHighlighter *m_highlighter;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/qt/qml/pollocad/icon.png"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    Executor executor;
    engine.rootContext()->setContextProperty("executor", &executor);

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
