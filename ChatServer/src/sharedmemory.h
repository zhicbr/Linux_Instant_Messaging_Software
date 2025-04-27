#ifndef SHAREDMEMORY_H
#define SHAREDMEMORY_H

#include <QObject>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <QString>

class SharedMemory : public QObject {
    Q_OBJECT
public:
    explicit SharedMemory(QObject *parent = nullptr);
    ~SharedMemory();

    // 创建或附加到共享内存
    bool create(const QString &key, size_t size);
    
    // 分离共享内存
    bool detach();
    
    // 写入数据到共享内存
    bool write(const QByteArray &data);
    
    // 从共享内存读取数据
    QByteArray read();
    
    // 获取共享内存大小
    size_t size() const { return m_size; }
    
    // 检查共享内存是否有效
    bool isValid() const { return m_shmid != -1 && m_memory != nullptr; }

private:
    int m_shmid;       // 共享内存ID
    void *m_memory;    // 共享内存指针
    size_t m_size;     // 共享内存大小
    key_t m_key;       // 共享内存键
};

#endif // SHAREDMEMORY_H
