#include <unordered_set>

#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSyntaxHighlighter>

#include "occtview.h"

#include "parser.h"
#include "backgroundexecutor.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/qt/qml/pollocadgui/res/icon.png"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    BackgroundExecutor executorManager;
    engine.rootContext()->setContextProperty("executor", &executorManager);

    QUrl fileToLoad;

    const auto args = QGuiApplication::arguments();
    if (args.length() >= 2) {
        fileToLoad = QUrl::fromLocalFile(args.at(1));
    }

    engine.rootContext()->setContextProperty("fileToLoad", fileToLoad);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("pollocadgui", "Main");

    return app.exec();
}
