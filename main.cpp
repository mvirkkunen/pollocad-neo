#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "parser.h"
#include "executor.h"

/*class ApplicationData : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE void parse(QString code) const {
        ::parse(code.toStdString());
    }
};*/

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    Executor executor;
    engine.rootContext()->setContextProperty("executor", &executor);

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
