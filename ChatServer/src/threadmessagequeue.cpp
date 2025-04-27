#include "threadmessagequeue.h"
#include <QDebug>

ThreadMessageQueue::ThreadMessageQueue(QObject *parent) : QObject(parent) {
}

ThreadMessageQueue::~ThreadMessageQueue() {
    clear();
}

void ThreadMessageQueue::enqueue(const QByteArray &message) {
    QMutexLocker locker(&m_mutex);
    
    // 添加消息到队列
    m_queue.enqueue(message);
    
    // 唤醒等待的线程
    m_condition.wakeOne();
    
    // 发出信号
    emit messageAvailable();
}

QByteArray ThreadMessageQueue::dequeue(bool wait, int timeout_ms) {
    QMutexLocker locker(&m_mutex);
    
    // 如果队列为空且需要等待
    if (m_queue.isEmpty() && wait) {
        if (timeout_ms < 0) {
            // 无限等待
            m_condition.wait(&m_mutex);
        } else {
            // 有超时的等待
            m_condition.wait(&m_mutex, timeout_ms);
        }
    }
    
    // 如果队列仍然为空，返回空数据
    if (m_queue.isEmpty()) {
        return QByteArray();
    }
    
    // 返回队列头部的消息
    return m_queue.dequeue();
}

bool ThreadMessageQueue::isEmpty() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.isEmpty();
}

int ThreadMessageQueue::size() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

void ThreadMessageQueue::clear() {
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
}
