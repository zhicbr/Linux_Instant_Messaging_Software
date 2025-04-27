#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QMap>
#include <QProcess>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

class ProcessManager : public QObject {
    Q_OBJECT
public:
    explicit ProcessManager(QObject *parent = nullptr);
    ~ProcessManager();

    // 启动一个新的子进程，返回进程ID
    pid_t startChildProcess(const QString &name, const QStringList &args = QStringList());
    
    // 终止指定的子进程
    bool terminateChildProcess(pid_t pid);
    
    // 终止所有子进程
    void terminateAllChildProcesses();
    
    // 检查子进程是否在运行
    bool isProcessRunning(pid_t pid);
    
    // 获取所有子进程的ID列表
    QList<pid_t> getAllChildProcesses() const;

    // 设置信号处理函数
    static void setupSignalHandlers();

private:
    QMap<pid_t, QString> m_childProcesses;
    
    // 信号处理函数
    static void signalHandler(int sig);
    static ProcessManager* s_instance;
};

#endif // PROCESSMANAGER_H
