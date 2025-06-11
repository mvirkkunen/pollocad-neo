#include <QGuiApplication>
#include <QTextDocument>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "executor.h"

class Util: public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void setupTextDocument(QQuickTextDocument *quickDoc) const {
        auto doc = quickDoc->textDocument();


    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    Executor executor;
    engine.rootContext()->setContextProperty("executor", &executor);

    Util util;
    engine.rootContext()->setContextProperty("util", &util);

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
