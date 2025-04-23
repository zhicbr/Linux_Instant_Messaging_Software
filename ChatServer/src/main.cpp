#include "server.h"
#include "logger.h"
#include "config.h"
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);
    
    // 初始化日志系统
    QString logPath = QCoreApplication::applicationDirPath() + "/" + Config::Logging::ServerLogDir;
    QDir logDir(logPath);
    if (!logDir.exists()) {
        logDir.mkpath(".");
    }
    
    QString logFilePath = logPath + "/server_" + 
                          QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    
    if (!Logger::init(logFilePath, static_cast<Logger::LogLevel>(Config::Logging::DefaultLogLevel))) {
        fprintf(stderr, "Failed to initialize logging system\n");
    } else {
        qInfo() << "服务器日志系统初始化成功，日志文件：" << logFilePath;
    }
    
    // 启动服务器
    Server server;
    server.start();
    
    int ret = a.exec();
    
    // 关闭日志
    Logger::close();
    
    return ret;
}
