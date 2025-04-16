#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "chatwindow.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    ChatWindow chatWindow;

    // 将 ChatWindow 暴露给 QML 作为上下文属性
    engine.rootContext()->setContextProperty("chatWindow", &chatWindow);

    const QUrl url(QStringLiteral("qrc:/qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
                         if (!obj && url == objUrl)
                             QCoreApplication::exit(-1);
                     }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}