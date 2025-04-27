#ifndef THREADMESSAGEQUEUE_H
#define THREADMESSAGEQUEUE_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QByteArray>

class ThreadMessageQueue : public QObject {
    Q_OBJECT
public:
    explicit ThreadMessageQueue(QObject *parent = nullptr);
    ~ThreadMessageQueue();

    // 发送消息
    void enqueue(const QByteArray &message);
    
    // 接收消息，如果队列为空，可以选择等待
    QByteArray dequeue(bool wait = true, int timeout_ms = -1);
    
    // 检查队列是否为空
    bool isEmpty() const;
    
    // 获取队列中的消息数量
    int size() const;
    
    // 清空队列
    void clear();

signals:
    // 当有新消息入队时发出信号
    void messageAvailable();

private:
    QQueue<QByteArray> m_queue;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
};

#endif // THREADMESSAGEQUEUE_H
