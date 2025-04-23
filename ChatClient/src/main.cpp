#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QDateTime>
#include "chatwindow.h"
#include "logger.h"
#include "config.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // 初始化日志系统
    QString logPath = QGuiApplication::applicationDirPath() + "/" + Config::Logging::ClientLogDir;
    QDir logDir(logPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    QString logFilePath = logPath + "/client_" + 
                          QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    
    if (!Logger::init(logFilePath, static_cast<Logger::LogLevel>(Config::Logging::DefaultLogLevel))) {
        fprintf(stderr, "Failed to initialize logging system\n");
    } else {
        qInfo() << "客户端日志系统初始化成功，日志文件：" << logFilePath;
    }

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

    int ret = app.exec();
    
    // 关闭日志系统
    Logger::close();
    
    return ret;
}