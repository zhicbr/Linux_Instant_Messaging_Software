#include "threadpool.h"
#include <QDebug>

Worker::Worker(QObject *parent) : QThread(parent), m_taskQueue(nullptr), m_mutex(nullptr), m_condition(nullptr), m_stop(false) {
}

Worker::~Worker() {
    stop();
    wait();
}

void Worker::setTaskQueue(QQueue<std::function<void()>> *taskQueue, QMutex *mutex, QWaitCondition *condition) {
    m_taskQueue = taskQueue;
    m_mutex = mutex;
    m_condition = condition;
}

void Worker::stop() {
    QMutexLocker locker(m_mutex);
    m_stop = true;
    m_condition->wakeAll();
}

void Worker::run() {
    qDebug() << "Worker thread started:" << QThread::currentThreadId();
    
    while (true) {
        std::function<void()> task;
        
        {
            QMutexLocker locker(m_mutex);
            
            // 等待任务或停止信号
            while (!m_stop && m_taskQueue->isEmpty()) {
                m_condition->wait(m_mutex);
            }
            
            // 检查是否需要停止
            if (m_stop) {
                break;
            }
            
            // 获取任务
            if (!m_taskQueue->isEmpty()) {
                task = m_taskQueue->dequeue();
            }
        }
        
        // 执行任务
        if (task) {
            try {
                task();
            } catch (const std::exception &e) {
                qDebug() << "Exception in worker thread:" << e.what();
            } catch (...) {
                qDebug() << "Unknown exception in worker thread";
            }
        }
    }
    
    qDebug() << "Worker thread stopped:" << QThread::currentThreadId();
}

ThreadPool::ThreadPool(QObject *parent) : QObject(parent), m_stop(false) {
}

ThreadPool::~ThreadPool() {
    m_stop = true;
    m_condition.wakeAll();
    
    for (Worker *worker : m_workers) {
        worker->stop();
        worker->wait();
        delete worker;
    }
}

bool ThreadPool::init(int threadCount) {
    if (threadCount <= 0) {
        qDebug() << "Invalid thread count:" << threadCount;
        return false;
    }
    
    // 创建工作线程
    for (int i = 0; i < threadCount; ++i) {
        Worker *worker = new Worker(this);
        worker->setTaskQueue(&m_taskQueue, &m_mutex, &m_condition);
        m_workers.append(worker);
        worker->start();
    }
    
    qDebug() << "Thread pool initialized with" << threadCount << "threads";
    return true;
}

void ThreadPool::addTask(const std::function<void()> &task) {
    QMutexLocker locker(&m_mutex);
    
    // 添加任务到队列
    m_taskQueue.enqueue(task);
    
    // 唤醒一个等待的线程
    m_condition.wakeOne();
}

void ThreadPool::waitForDone() {
    QMutexLocker locker(&m_mutex);
    
    // 等待任务队列为空
    while (!m_taskQueue.isEmpty()) {
        m_condition.wait(&m_mutex);
    }
}

int ThreadPool::pendingTaskCount() {
    QMutexLocker locker(&m_mutex);
    return m_taskQueue.size();
}
