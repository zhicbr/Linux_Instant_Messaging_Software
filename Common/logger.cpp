#include "logger.h"
#include "config.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QtGlobal>
#include <QDebug>
#include <QRegularExpression>

// 静态成员初始化
QFile* Logger::logFile = nullptr;
QTextStream* Logger::logStream = nullptr;
QMutex Logger::mutex;
Logger::LogLevel Logger::currentLevel = static_cast<Logger::LogLevel>(Config::Logging::DefaultLogLevel);
bool Logger::initialized = false;
QString Logger::currentLogPath;
qint64 Logger::maxLogFileSize = Config::Logging::MaxLogFileSize * 1024 * 1024; // 转换为字节
int Logger::maxLogFileCount = Config::Logging::MaxLogFileCount;

bool Logger::init(const QString &logFilePath, LogLevel level)
{
    if (initialized) {
        return true; // 已经初始化
    }

    QMutexLocker locker(&mutex);
    
    // 确保目录存在
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    currentLogPath = logFilePath;
    currentLevel = level;
    
    // 打开日志文件
    logFile = new QFile(logFilePath);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete logFile;
        logFile = nullptr;
        return false;
    }
    
    // 创建文本流
    logStream = new QTextStream(logFile);
    
    // 安装消息处理器
    qInstallMessageHandler(messageHandler);
    
    initialized = true;
    
    // 记录启动日志
    *logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
              << " [INFO] " << "日志系统已初始化" << Qt::endl;
    logStream->flush();
    
    return true;
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&mutex);
    currentLevel = level;
}

void Logger::close()
{
    QMutexLocker locker(&mutex);
    
    if (!initialized) {
        return;
    }
    
    // 记录关闭日志
    *logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
              << " [INFO] " << "日志系统已关闭" << Qt::endl;
    logStream->flush();
    
    // 恢复默认消息处理器
    qInstallMessageHandler(0);
    
    // 关闭和清理资源
    delete logStream;
    logStream = nullptr;
    
    logFile->close();
    delete logFile;
    logFile = nullptr;
    
    initialized = false;
}

void Logger::checkLogRotation()
{
    QMutexLocker locker(&mutex);
    
    if (!initialized || !logFile) {
        return;
    }
    
    // 检查日志文件大小
    if (logFile->size() >= maxLogFileSize) {
        rotateLogFiles();
    }
}

void Logger::rotateLogFiles()
{
    if (!initialized || !logFile || currentLogPath.isEmpty()) {
        return;
    }
    
    // 关闭当前日志文件
    logStream->flush();
    delete logStream;
    logStream = nullptr;
    logFile->close();
    delete logFile;
    logFile = nullptr;
    
    QFileInfo fileInfo(currentLogPath);
    QString baseName = fileInfo.baseName();
    QString suffix = fileInfo.suffix();
    QString dirPath = fileInfo.absolutePath();
    
    // 查找当前所有日志文件
    QDir dir(dirPath);
    QStringList filters;
    filters << baseName + ".*." + suffix;
    QStringList logFiles = dir.entryList(filters, QDir::Files, QDir::Time);
    
    // 正则表达式提取日志文件序号
    QRegularExpression regex(baseName + "\\.(\\d+)\\." + suffix);
    
    // 将最旧的日志文件删除（如果已达到最大文件数）
    if (logFiles.size() >= maxLogFileCount - 1) {
        int oldestIndex = 0;
        int maxIndex = 0;
        
        for (const QString &fileName : logFiles) {
            QRegularExpressionMatch match = regex.match(fileName);
            if (match.hasMatch()) {
                int index = match.captured(1).toInt();
                if (index > maxIndex) {
                    maxIndex = index;
                }
                if (index < oldestIndex || oldestIndex == 0) {
                    oldestIndex = index;
                }
            }
        }
        
        if (oldestIndex > 0) {
            QString oldestFile = dirPath + "/" + baseName + "." + QString::number(oldestIndex) + "." + suffix;
            QFile::remove(oldestFile);
        }
    }
    
    // 重命名当前日志文件
    QString timestamp = QString::number(QDateTime::currentDateTime().toSecsSinceEpoch());
    QString newFileName = dirPath + "/" + baseName + "." + timestamp + "." + suffix;
    QFile::rename(currentLogPath, newFileName);
    
    // 创建新的日志文件
    logFile = new QFile(currentLogPath);
    if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete logFile;
        logFile = nullptr;
        initialized = false;
        return;
    }
    
    logStream = new QTextStream(logFile);
    
    // 记录日志轮转信息
    *logStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") 
              << " [INFO] " << "日志文件已轮转，上一个日志文件已保存为 " << newFileName << Qt::endl;
    logStream->flush();
}

Logger::LogLevel Logger::qtMsgTypeToLogLevel(QtMsgType type)
{
    switch (type) {
        case QtDebugMsg:
            return Debug;
        case QtInfoMsg:
            return Info;
        case QtWarningMsg:
            return Warning;
        case QtCriticalMsg:
            return Error;
        case QtFatalMsg:
            return Fatal;
        default:
            return Debug;
    }
}

QString Logger::logLevelToString(LogLevel level)
{
    switch (level) {
        case Debug:
            return "DEBUG";
        case Info:
            return "INFO";
        case Warning:
            return "WARN";
        case Error:
            return "ERROR";
        case Fatal:
            return "FATAL";
        default:
            return "DEBUG";
    }
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // 转换Qt消息类型为Logger日志级别
    LogLevel msgLevel = qtMsgTypeToLogLevel(type);
    
    // 低于当前级别的日志不记录
    if (msgLevel < currentLevel) {
        return;
    }
    
    QMutexLocker locker(&mutex);
    
    if (!initialized || !logStream) {
        return;
    }
    
    // 检查是否需要进行日志轮转
    if (logFile->size() >= maxLogFileSize) {
        rotateLogFiles();
    }
    
    // 构建日志信息
    QString logMessage = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    logMessage += QString(" [%1] ").arg(logLevelToString(msgLevel));
    
    // 添加文件和行号（如果可用）
    if (context.file && strlen(context.file) > 0) {
        logMessage += QString("(%1:%2) ").arg(context.file).arg(context.line);
    }
    
    // 添加函数名（如果可用）
    if (context.function && strlen(context.function) > 0) {
        logMessage += QString("{%1} ").arg(context.function);
    }
    
    // 添加日志消息
    logMessage += msg;
    
    // 写入日志文件
    *logStream << logMessage << Qt::endl;
    logStream->flush();
    
    // 对于严重错误，确保立即刷新到磁盘
    if (type == QtFatalMsg) {
        logFile->flush();
    }
    
    // 同时输出到控制台
    if (Config::Logging::LogToConsole) {
        fprintf(stderr, "%s\n", qPrintable(logMessage));
    }
    
    // 对于致命错误，终止应用程序
    if (type == QtFatalMsg) {
        abort();
    }
} 