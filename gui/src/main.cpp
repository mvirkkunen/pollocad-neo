#include <unordered_set>

#include <QFile>
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
    engine.loadFromModule("pollocadgui", "Main");

    return app.exec();
}
