#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

namespace Config {
    // 网络配置
    static const quint16 DefaultPort = 12345;
    static const QString DefaultServerIp = "127.0.0.1";
    
    // 日志配置
    namespace Logging {
        // 是否在控制台同时显示日志
        static const bool LogToConsole = true;
        
        // 默认日志级别
        // 0: Debug, 1: Info, 2: Warning, 3: Error, 4: Fatal
        static const int DefaultLogLevel = 0;
        
        // 日志文件最大大小（MB）
        static const int MaxLogFileSize = 10;
        
        // 保留的日志文件数量
        static const int MaxLogFileCount = 10;
        
        // 日志文件路径
        static const QString ServerLogDir = "logs/server";
        static const QString ClientLogDir = "logs/client";
    }
}

#endif
