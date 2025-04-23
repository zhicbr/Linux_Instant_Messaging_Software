#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QVector>

class Logger
{
public:
    // 日志级别
    enum LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    // 初始化日志系统
    static bool init(const QString &logFilePath, LogLevel level = Debug);

    // 设置日志级别
    static void setLogLevel(LogLevel level);

    // 关闭日志系统
    static void close();
    
    // 检查日志文件大小并执行日志轮转
    static void checkLogRotation();

private:
    static QFile* logFile;
    static QTextStream* logStream;
    static QMutex mutex;
    static LogLevel currentLevel;
    static bool initialized;
    static QString currentLogPath;
    static qint64 maxLogFileSize;
    static int maxLogFileCount;
    
    // 执行日志轮转
    static void rotateLogFiles();
    
    // 消息处理函数
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    
    // 转换Qt消息类型到Logger日志级别
    static LogLevel qtMsgTypeToLogLevel(QtMsgType type);
    
    // 获取日志级别对应的字符串
    static QString logLevelToString(LogLevel level);
};

#endif // LOGGER_H 