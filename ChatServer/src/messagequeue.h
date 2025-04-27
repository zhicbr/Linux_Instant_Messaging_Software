#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <QObject>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <QString>

// 消息结构体
struct MessageData {
    long mtype;           // 消息类型
    char mtext[1024];     // 消息内容
};

class MessageQueue : public QObject {
    Q_OBJECT
public:
    explicit MessageQueue(QObject *parent = nullptr);
    ~MessageQueue();

    // 创建或打开消息队列
    bool create(const QString &key);
    
    // 关闭消息队列
    bool close();
    
    // 发送消息
    bool send(long type, const QByteArray &data);
    
    // 接收消息
    QByteArray receive(long type, bool *ok = nullptr);
    
    // 检查消息队列是否有效
    bool isValid() const { return m_msgid != -1; }

private:
    int m_msgid;      // 消息队列ID
    key_t m_key;      // 消息队列键
};

#endif // MESSAGEQUEUE_H
