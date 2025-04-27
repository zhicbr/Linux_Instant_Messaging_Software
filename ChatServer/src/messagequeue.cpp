#include "messagequeue.h"
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <string.h>
#include <errno.h>

MessageQueue::MessageQueue(QObject *parent) : QObject(parent), m_msgid(-1), m_key(0) {
}

MessageQueue::~MessageQueue() {
    close();
}

bool MessageQueue::create(const QString &key) {
    // 如果已经打开消息队列，先关闭
    if (isValid()) {
        close();
    }

    // 生成唯一键
    // 确保文件存在，因为ftok需要一个存在的文件
    QString keyFile = "/tmp/chat_server_msgq_key";
    QFile file(keyFile);
    if (!file.exists()) {
        file.open(QIODevice::WriteOnly);
        file.close();
    }

    m_key = ftok(keyFile.toLocal8Bit().constData(), 'M');
    if (m_key == -1) {
        qDebug() << "Failed to generate key:" << strerror(errno);
        return false;
    }

    // 创建消息队列
    m_msgid = msgget(m_key, IPC_CREAT | 0666);
    if (m_msgid == -1) {
        qDebug() << "Failed to create message queue:" << strerror(errno);
        return false;
    }

    qDebug() << "Created message queue with key" << m_key;
    return true;
}

bool MessageQueue::close() {
    if (m_msgid != -1) {
        // 标记消息队列可以被删除
        if (msgctl(m_msgid, IPC_RMID, nullptr) == -1) {
            qDebug() << "Failed to mark message queue for deletion:" << strerror(errno);
            return false;
        }
        m_msgid = -1;
    }

    return true;
}

bool MessageQueue::send(long type, const QByteArray &data) {
    if (!isValid()) {
        qDebug() << "Message queue is not valid";
        return false;
    }

    if (data.size() > sizeof(MessageData::mtext)) {
        qDebug() << "Data size" << data.size() << "exceeds message size limit" << sizeof(MessageData::mtext);
        return false;
    }

    // 准备消息
    MessageData msg;
    msg.mtype = type;
    memcpy(msg.mtext, data.constData(), data.size());

    // 发送消息
    if (msgsnd(m_msgid, &msg, data.size(), 0) == -1) {
        qDebug() << "Failed to send message:" << strerror(errno);
        return false;
    }

    return true;
}

QByteArray MessageQueue::receive(long type, bool *ok) {
    if (!isValid()) {
        qDebug() << "Message queue is not valid";
        if (ok) *ok = false;
        return QByteArray();
    }

    // 准备接收消息
    MessageData msg;
    ssize_t size = msgrcv(m_msgid, &msg, sizeof(msg.mtext), type, 0);

    if (size == -1) {
        qDebug() << "Failed to receive message:" << strerror(errno);
        if (ok) *ok = false;
        return QByteArray();
    }

    if (ok) *ok = true;
    return QByteArray(msg.mtext, size);
}
