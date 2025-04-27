#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QList>
#include <functional>

// 工作线程类
class Worker : public QThread {
    Q_OBJECT
public:
    Worker(QObject *parent = nullptr);
    ~Worker();

    // 设置任务队列和相关同步对象
    void setTaskQueue(QQueue<std::function<void()>> *taskQueue, QMutex *mutex, QWaitCondition *condition);
    
    // 停止线程
    void stop();

protected:
    void run() override;

private:
    QQueue<std::function<void()>> *m_taskQueue;
    QMutex *m_mutex;
    QWaitCondition *m_condition;
    bool m_stop;
};

// 线程池类
class ThreadPool : public QObject {
    Q_OBJECT
public:
    explicit ThreadPool(QObject *parent = nullptr);
    ~ThreadPool();

    // 初始化线程池
    bool init(int threadCount = QThread::idealThreadCount());
    
    // 添加任务到线程池
    void addTask(const std::function<void()> &task);
    
    // 等待所有任务完成
    void waitForDone();
    
    // 获取线程池大小
    int size() const { return m_workers.size(); }
    
    // 获取等待中的任务数量
    int pendingTaskCount();

private:
    QList<Worker*> m_workers;
    QQueue<std::function<void()>> m_taskQueue;
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_stop;
};

#endif // THREADPOOL_H
