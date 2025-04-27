#include "processmanager.h"
#include <QDebug>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

ProcessManager* ProcessManager::s_instance = nullptr;

ProcessManager::ProcessManager(QObject *parent) : QObject(parent) {
    s_instance = this;
}

ProcessManager::~ProcessManager() {
    terminateAllChildProcesses();
    s_instance = nullptr;
}

pid_t ProcessManager::startChildProcess(const QString &name, const QStringList &args) {
    pid_t pid = fork();
    
    if (pid < 0) {
        // 创建进程失败
        qDebug() << "Failed to fork process:" << strerror(errno);
        return -1;
    } else if (pid == 0) {
        // 子进程
        qDebug() << "Child process started with PID:" << getpid();
        
        // 准备参数
        QVector<char*> argv;
        QByteArray nameBytes = name.toLocal8Bit();
        argv.append(nameBytes.data());
        
        for (const QString &arg : args) {
            QByteArray argBytes = arg.toLocal8Bit();
            argv.append(argBytes.data());
        }
        
        argv.append(nullptr);
        
        // 执行新程序
        execvp(nameBytes.data(), argv.data());
        
        // 如果execvp返回，说明出错了
        qDebug() << "Failed to execute" << name << ":" << strerror(errno);
        exit(1);
    } else {
        // 父进程
        qDebug() << "Started child process" << name << "with PID:" << pid;
        m_childProcesses[pid] = name;
        return pid;
    }
}

bool ProcessManager::terminateChildProcess(pid_t pid) {
    if (!m_childProcesses.contains(pid)) {
        return false;
    }
    
    // 发送SIGTERM信号
    if (kill(pid, SIGTERM) != 0) {
        qDebug() << "Failed to terminate process" << pid << ":" << strerror(errno);
        return false;
    }
    
    // 等待子进程结束
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        qDebug() << "Failed to wait for process" << pid << ":" << strerror(errno);
        return false;
    }
    
    m_childProcesses.remove(pid);
    qDebug() << "Process" << pid << "terminated";
    return true;
}

void ProcessManager::terminateAllChildProcesses() {
    QList<pid_t> pids = m_childProcesses.keys();
    for (pid_t pid : pids) {
        terminateChildProcess(pid);
    }
}

bool ProcessManager::isProcessRunning(pid_t pid) {
    if (!m_childProcesses.contains(pid)) {
        return false;
    }
    
    // 发送信号0检查进程是否存在
    return kill(pid, 0) == 0;
}

QList<pid_t> ProcessManager::getAllChildProcesses() const {
    return m_childProcesses.keys();
}

void ProcessManager::setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = &ProcessManager::signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    // 注册SIGINT和SIGTERM信号处理函数
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    
    // 忽略SIGPIPE信号，防止在写入已关闭的socket时程序终止
    signal(SIGPIPE, SIG_IGN);
}

void ProcessManager::signalHandler(int sig) {
    qDebug() << "Received signal:" << sig;
    
    if (s_instance) {
        s_instance->terminateAllChildProcesses();
    }
    
    // 退出程序
    exit(0);
}
